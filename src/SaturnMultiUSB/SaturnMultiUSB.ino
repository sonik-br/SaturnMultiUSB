/*******************************************************************************
 * Sega Saturn controllers to USB using an Arduino Leonardo.
 * 
 * Works with digital pad and analog pad.
 * 
 * By using the multitap it's possible to connect up to 7 controllers.
 * 
 * Also works with MegaDrive controllers and mulltitaps.
 * 
 * It uses the ArduinoJoystickLibrary and the DigitalWriteFast Library.
 * 
 * For details on Joystick Library, see
 * https://github.com/MHeironimus/ArduinoJoystickLibrary
 * 
 * For details on Digital Write Fast Library, see
 * https://github.com/NicksonYap/digitalWriteFast
*/


//Arduino Joystick Library
#include <Joystick.h>

//Arduino Digital Write Fast Library
#include <digitalWriteFast.h>

//#define ENABLE_SERIAL_DEBUG

#define SATURN_PORTS 2 //1 or 2 controller ports

//Saturn port 1
#define SAT1_TH A2
#define SAT1_TR 7
#define SAT1_D0 A4
#define SAT1_D1 A5
#define SAT1_D2 A3
#define SAT1_D3 A0
#define SAT1_TL A1
#define SAT1_PINGROUP PINF

//Saturn port 2
#define SAT2_TH 8
#define SAT2_TR 9
#define SAT2_D0 2
#define SAT2_D1 3
#define SAT2_D2 4
#define SAT2_D3 6
#define SAT2_TL 5
#define SAT2_PINGROUP PIND

#define ENABLE_8BITDO_HOME_BTN//coment to disable the use of HOME button on 8bidto M30 2.4G.


#define MAX_USB_STICKS 5 + SATURN_PORTS //maximum 7 controllers per arduino

#ifdef ENABLE_SERIAL_DEBUG
#define dstart(spd) do {Serial.begin (spd); while (!Serial) {digitalWrite (LED_BUILTIN, (millis () / 500) % 2);}} while (0);
#define debug(...) Serial.print (__VA_ARGS__)
#define debugln(...) Serial.println (__VA_ARGS__)
#else
#define dstart(...)
#define debug(...)
#define debugln(...)
#endif

unsigned int sleepTime = 4000; //In micro seconds

Joystick_* usbStick[MAX_USB_STICKS];
uint8_t lastState[MAX_USB_STICKS][8]= {}; //Store the state of each controller

uint8_t joyStateChanged = B00000000; //Store if each joystick has changed state

enum DB9_TR_Enum {
  DB9_TR_INPUT = 0,
  DB9_TR_OUTPUT
};

DB9_TR_Enum portState[2] = { DB9_TR_INPUT, DB9_TR_INPUT };


short totalUsb = SATURN_PORTS;
short joyCount = 0;
short currentPort = 1;
int16_t hatTable[16];

unsigned long start;
unsigned long end;
unsigned long delta;

void setTR_High() {
  if (currentPort == 1) {
    digitalWriteFast(SAT1_TR, HIGH);
  } else {
    digitalWriteFast(SAT2_TR, HIGH);
  }
}

void setTR_Low() {
  if (currentPort == 1) {
    digitalWriteFast(SAT1_TR, LOW);
  } else {
    digitalWriteFast(SAT2_TR, LOW);
  }
}

void setTH_High() {
  if (currentPort == 1) {
    digitalWriteFast(SAT1_TH, HIGH);
  } else {
    digitalWriteFast(SAT2_TH, HIGH);
  }
}

void setTH_Low() {
  if (currentPort == 1) {
    digitalWriteFast(SAT1_TH, LOW);
  } else {
    digitalWriteFast(SAT2_TH, LOW);
  }
}


bool readTL() {
  if (currentPort == 1) {
    return digitalReadFast(SAT1_TL);
  } else {
    return digitalReadFast(SAT2_TL);
  }
}

char waitTL(char state) {
  char t_out = 100;
  if (state) {
    while (!readTL()) {
      delayMicroseconds(4);
      t_out--;
      if (!t_out)
        return -1;
    }
  } else {
    while (readTL()) {
      delayMicroseconds(4);
      t_out--;
      if (!t_out)
        return -1;
    }
  }
  return 0;
}

char readMegadriveBits() { //6 bits
  bool TL;
  bool TR;
  uint8_t nibble = readNibble();

  if(portState[currentPort-1] != DB9_TR_OUTPUT) {//saturn dont need these values
    if (currentPort == 1) {
      TL = digitalReadFast(SAT1_TL);
      TR = digitalReadFast(SAT1_TR);
    } else {
      TL = digitalReadFast(SAT2_TL);
      TR = digitalReadFast(SAT2_TR);
    }
  
    bitWrite(nibble, 4, TL);
    bitWrite(nibble, 5, TR);
  }        
  return nibble;
}

char readNibble() {
  //Read from port 1 or port 2
  uint8_t nibble = currentPort == 1 ? SAT1_PINGROUP : SAT2_PINGROUP;
  return ((nibble >> 4) & B00001000) + // D3
         ((nibble >> 2) & B00000100) + // D2
         ((nibble << 1) & B00000010) + // D1
         ((nibble >> 1) & B00000001);  // D0
}

void setTR_Mode(DB9_TR_Enum mode) {
  if(portState[currentPort-1] != mode) {
    portState[currentPort-1] = mode;
    if(mode == DB9_TR_OUTPUT) {
      if (currentPort == 1)
        pinMode(SAT1_TR, OUTPUT);
      else
        pinMode(SAT2_TR, OUTPUT);
      setTR_High();
    } else {
      if (currentPort == 1)
        pinMode(SAT1_TR, INPUT_PULLUP);
      else
        pinMode(SAT2_TR, INPUT_PULLUP);
    }
  }
}

__attribute__ ((noinline))
void readSatPort() {
  uint8_t nibble_0;
  uint8_t nibble_1;

  setTH_High();
  delayMicroseconds(4);
  nibble_0 = readMegadriveBits();

  setTH_Low();
  delayMicroseconds(4);
  nibble_1 = readMegadriveBits();

  if ((nibble_0 & B00000111) == B00000100) { //Saturn ID
    //debugln (F("DIGITAL-PAD"));
    setTR_Mode(DB9_TR_OUTPUT);
    readDigitalPad(nibble_0 & B00001111, nibble_1 & B00001111);
  } else if ((nibble_0 & B00001111) == B00000001 && (nibble_1 & B00001111) == B00000001) { //Saturn ID
    //debugln (F("3-WIRE-HANDSHAKE"));
    setTR_Mode(DB9_TR_OUTPUT);
    readThreeWire();
  } else if((nibble_1 & B00001100) == B00000000) { //Megadrive ID
    //debugln (F("MEGADRIVE"));
    setTR_Mode(DB9_TR_INPUT);
    readMegadrivePad(nibble_0, nibble_1);
  } else if ((nibble_0 & B00001111) == B00000011 && (nibble_1 & B00001111) == B00001111) { //Megadrive multitap
    setTR_Mode(DB9_TR_OUTPUT);
    //debugln (F("MEGADRIVE MULTITAP"));
    readMegaMultiTap();
  } else {
    setTR_Mode(DB9_TR_INPUT);
  }

  setTH_High();

  if (portState[currentPort-1] == DB9_TR_OUTPUT)
    setTR_High();
  
  delayMicroseconds(100);
}

void readMegaMultiTap() {
  byte joyIndex;
  uint8_t nibble_0;
  uint8_t nibble_1;
  uint16_t temp;
  uint8_t nibbles;
  char tr = 0;
  char r;
  int i;

  //read first two nibbles. should be zero and zero
  for (int i = 0; i < 2; i++) {
    if (tr) {
      setTR_High();
      r = waitTL(1);
      if (r)
        return;
    } else {
      setTR_Low();
      r = waitTL(0);
      if (r)
        return;
    }

    delayMicroseconds(4);

    tr ^= 1;
  }

  //read each port controller ID and stores the 4 values in the 16 bits variable
  for (i = 0; i < 16; i+=4) {
    if (tr) {
      setTR_High();
      r = waitTL(1);
      if (r)
        return;
    } else {
      setTR_Low();
      r = waitTL(0);
      if (r)
        return;
    }

    delayMicroseconds(4);
    temp |= (readNibble() << i);

    tr ^= 1;
  }

  //now read each port
  for (i = 0; i < 16; i+=4) {
    nibble_0 = (temp >> i) & B1111;

    if (nibble_0 == B0000) //3 button. 2 nibbles
      nibbles = 2;
    else if (nibble_0 == B0001) //6 buttons. 3 nibbles
      nibbles = 3;
    else if (nibble_0 == B0010) //mouse. 6 nibbles
      nibbles = 6;
    else //non-connection. 0 reads
      continue;

    if (nibbles != 6)//ignore mouse
      joyIndex = joyCount++;
    
    for(int x = 0; x < nibbles; x++) {
      if (tr) {
        setTR_High();
        r = waitTL(1);
        if (r)
          return;
      } else {
        setTR_Low();
        r = waitTL(0);
        if (r)
          return;
      }
      
      if (nibbles != 6)//ignore mouse
        setUsbValues(joyIndex, false, x, readNibble());
      
      tr ^= 1;
    }
    
    delayMicroseconds(40);
  }
}

void readThreeWire() {
  uint8_t nibble_0;
  uint8_t nibble_1;
  char tr = 0;
  char r;

  //setTR_High()
  //delayMicroseconds(4);

  for (int i = 0; i < 2; i++) {
    if (tr) {
      setTR_High();
      r = waitTL(1);
      if (r)
        return;
    } else {
      setTR_Low();
      r = waitTL(0);
      if (r)
        return;
    }

    delayMicroseconds(4);

    //nibbleTMP = readNibble();

    if (i == 0)
      nibble_0 = readNibble();
    else
      nibble_1 = readNibble();

    tr ^= 1;
  }

  if (nibble_0 == B00000100 && nibble_1 == B00000001) {
    //debugln (F("6P MULTI-TAP"));
    readMultitap();
  } else if (nibble_0 == B00000000 && nibble_1 == B00000010) {
    //debugln (F("DIGITAL"));
    readThreeWireController(false);
  } else if (nibble_0 == B00000001 && nibble_1 == B00000110) {
    //debugln (F("ANALOG"));
    readThreeWireController(true);
  } else if (nibble_0 == B00001110 && nibble_1 < B00000011) { //megadrive 3 or 6 button pad. ignore mouse
    //debugln (F("MEGADRIVE"));
    readThreeWireController(false);
  } else if (nibble_0 == B00001111 && nibble_1 == B00001111) {
    //debugln (F("NONE"));
  } else {
    //debugln (F("UNKNOWN"));
    readUnhandledPeripheral(nibble_1);
  }
}

void readMultitap() {
  int port;
  int i;
  int nibbles = 2;
  uint8_t nibbleTMP;
  uint8_t nibble_0;
  uint8_t nibble_1;
  char tr = 0;
  char r;

  //setTR_High()
  //delayMicroseconds(10);


  //Multitap header is fixed: 6 (ports) and zero (len)
  for (i = 0; i < nibbles; i++) {
    if (tr) {
      setTR_High();
      r = waitTL(1);
      if (r)
        return;
    }
    else {
      setTR_Low();
      r = waitTL(0);
      if (r)
        return;
    }
    delayMicroseconds(2);
    tr ^= 1;
  }
  //delayMicroseconds(5);


  for (i = 0; i < 6; i++) {// Read the 6 multitap ports
    readThreeWire();
    delayMicroseconds(4);
  }

  setTR_Low();
  r = waitTL(0);
  if (r)
    return;

  //nibbleTMP = readNibble();

  setTR_High();
  r = waitTL(1);
  if (r)
    return;

  //nibbleTMP = readNibble();
  //delayMicroseconds(5);
}

void readUnhandledPeripheral(int len) {
  int nibbles = len * 2;
  char tr = 0;
  char r;

  for (int i = 0; i < nibbles; i++) {
    if (tr) {
      setTR_High();
      r = waitTL(1);
      if (r)
        return;
    } else {
      setTR_Low();
      r = waitTL(0);
      if (r)
        return;
    }
    delayMicroseconds(2);

    //readNibble();
    tr ^= 1;
  }
}

void setUsbValues(int joyIndex, bool isAnalog, int page, uint8_t nibbleTMP) {
  //Only report L and R for digital pads. analog will use brake and throttle.

  if (joyIndex >= totalUsb)
    return;

  //Skip if no change in state
  if(lastState[joyIndex][page] == nibbleTMP)
    return;

  if (page == 0) { //HAT RLDU
    usbStick[joyIndex]->setHatSwitch(0, hatTable[nibbleTMP]);
  } else if (page == 1) { //SACB
    usbStick[joyIndex]->setButton(1, !(nibbleTMP & B00000100)); //A
    usbStick[joyIndex]->setButton(2, !(nibbleTMP & B00000001)); //B
    usbStick[joyIndex]->setButton(5, !(nibbleTMP & B00000010)); //C
    usbStick[joyIndex]->setButton(9, !(nibbleTMP & B00001000)); //S
  } else if (page == 2) { //RXYZ
    usbStick[joyIndex]->setButton(0, !(nibbleTMP & B00000100)); //X
    usbStick[joyIndex]->setButton(3, !(nibbleTMP & B00000010)); //Y
    usbStick[joyIndex]->setButton(4, !(nibbleTMP & B00000001)); //Z
    if (!isAnalog)
      usbStick[joyIndex]->setButton(7, !(nibbleTMP & B00001000)); //R
  } else if (page == 3 && !isAnalog) {
    usbStick[joyIndex]->setButton(6, !(nibbleTMP & B00001000)); //L
  } else if (page == 4) { //X axis
    usbStick[joyIndex]->setXAxis(nibbleTMP);
  } else if (page == 5) { //Y axis
    usbStick[joyIndex]->setYAxis(nibbleTMP);
  } else if (page == 6) { //RT axis
    usbStick[joyIndex]->setThrottle(nibbleTMP);
  } else if (page == 7) { //LT axis
    usbStick[joyIndex]->setBrake(nibbleTMP);
  }
  
  //update lastState for current joyIndex
  lastState[joyIndex][page] = nibbleTMP;
  
  //state changed for current joyIndex
  bitWrite(joyStateChanged, joyIndex, 1);
}

void readThreeWireController(bool isAnalog) {
  byte joyIndex = joyCount++;
  int i;
  int nibbles = (isAnalog ? 12 : 4);
  uint8_t nibbleTMP;
  uint8_t nibble_0;
  uint8_t nibble_1;
  char tr = 0;
  char r;

  for (i = 0; i < nibbles; i++) {
    if (tr) {
      setTR_High();
      r = waitTL(1);
      if (r)
        return;
    } else {
      setTR_Low();
      r = waitTL(0);
      if (r)
        return;
    }
    delayMicroseconds(2);

    nibbleTMP = readNibble();
    tr ^= 1;

    if (i < 3) {
      setUsbValues(joyIndex, isAnalog, i, nibbleTMP);
    } else if(i == 3) { //L
      nibbleTMP &= B00001000;
      setUsbValues(joyIndex, isAnalog, i, nibbleTMP);
    } else if (i == 4) { //X axis
      nibble_0 = nibbleTMP;
    } else if (i == 5) { //X axis
      nibble_1 = (nibble_0 & 0xF) << 4 | (nibbleTMP & 0xF);
      setUsbValues(joyIndex, isAnalog, 4, nibble_1);
    } else if (i == 6) { //Y axis
      nibble_0 = nibbleTMP;
    } else if (i == 7) { //Y axis
      nibble_1 = (nibble_0 & 0xF) << 4 | (nibbleTMP & 0xF);
      setUsbValues(joyIndex, isAnalog, 5, nibble_1);
    } else if (i == 8) { //RT analog
      nibble_0 = nibbleTMP;
    } else if (i == 9) { //RT analog
      nibble_1 = (nibble_0 & 0xF) << 4 | (nibbleTMP & 0xF);
      setUsbValues(joyIndex, isAnalog, 6, nibble_1);
    } else if (i == 10) { //LT analog
      nibble_0 = nibbleTMP;
    } else if (i == 11) { //LT analog
      nibble_1 = (nibble_0 & 0xF) << 4 | (nibbleTMP & 0xF);
      setUsbValues(joyIndex, isAnalog, 7, nibble_1);
    }
  }
}

void readMegadrivePad(uint8_t nibble_0, uint8_t nibble_1) {
  byte joyIndex = joyCount++;

  // RLDU
  setUsbValues(joyIndex, false, 0, nibble_0 & B00001111);

  // SACB
  setUsbValues(joyIndex, false, 1, (nibble_0 >> 4) | (nibble_1 >> 2));

  //check if is 6-button pad;
  setTH_High();
  delayMicroseconds(4);
  setTH_Low();
  delayMicroseconds(4);

  setTH_High();
  delayMicroseconds(4);
  setTH_Low();
  delayMicroseconds(4);
  
  nibble_0 = readMegadriveBits();
  
  if ((nibble_0 & B00001111) == B0) { //it is a 6-button pad
    setTH_High();
    delayMicroseconds(4);
    nibble_0 = readMegadriveBits();

    setTH_Low();
    delayMicroseconds(4);
    nibble_1 = readMegadriveBits();

    //11MXYZ
    setUsbValues(joyIndex, false, 2, nibble_0 & B00001111);

    //SA1111
    //8bitdo M30 extra buttons
    //111S1H STAR and HOME
    
    //use HOME button as the missing L from saturn
#ifdef ENABLE_8BITDO_HOME_BTN
    setUsbValues(joyIndex, false, 3, nibble_1 << 3);
#endif
  }
}

void readDigitalPad(uint8_t nibble_0, uint8_t nibble_1) {
  byte joyIndex = joyCount++;
  uint8_t nibbleTMP;
  //uint8_t currentHatState;

  // L100
  //debugln (firstNibble, BIN);
  setUsbValues(joyIndex, false, 3, nibble_0);

  // RLDU
  //setTH_Low();
  //setTR_High();
  //delayMicroseconds(4);
  //nibbleTMP = readNibble();
  //debugln (nibble_1, BIN);
  setUsbValues(joyIndex, false, 0, nibble_1);

  // SACB
  setTH_High();
  setTR_Low();
  delayMicroseconds(4);
  nibbleTMP = readNibble();
  //debugln (nibbleTMP, BIN);
  setUsbValues(joyIndex, false, 1, nibbleTMP);

  // RYXZ
  setTH_Low();
  setTR_Low();
  delayMicroseconds(4);
  nibbleTMP = readNibble();
  //debugln (nibbleTMP, BIN);
  setUsbValues(joyIndex, false, 2, nibbleTMP);
}

void reportUsb() {
  for (int i = 0; i < joyCount; i++) {
    if (i >= totalUsb)
      break;
    if (bitRead(joyStateChanged, i))
      usbStick[i]->sendState();
  }
}


//Call only on arduino power-on to check for multitap connected
void detectMultitap() {
  uint8_t nibble_0;
  uint8_t nibble_1;
  uint8_t nibbleTMP;
  int nibbles = 2;
  int i;
  char tr = 0;
  char r;

  //setTH_High();
  //setTR_High();
  //delayMicroseconds(4);

  r = waitTL(1); //Device is not ready yet. Saturn and Mega multitap hold TL high while initializing.
  if (r)
    return;

  //readMegadriveBits
  nibble_0 = readNibble();

  setTH_Low();
  delayMicroseconds(4);
  nibble_1 = readNibble();

  if ((nibble_0 & B00001111) == B00000011 && (nibble_1 & B00001111) == B00001111) { //Megadrive multitap
    setTR_Mode(DB9_TR_OUTPUT);
    totalUsb += 3;
  } else if (nibble_0 == B00000001 && nibble_1 == B00000001) { //SATURN 3-WIRE-HANDSHAKE
    setTR_Mode(DB9_TR_OUTPUT);
    for (i = 0; i < nibbles; i++) {
      if (tr) {
        setTR_High();
        r = waitTL(1);
        if (r)
          return;
      } else {
        setTR_Low();
        r = waitTL(0);
        if (r)
          return;
      }
      delayMicroseconds(4);

      nibbleTMP = readNibble();

      if (i == 0)
        nibble_0 = nibbleTMP;
      else
        nibble_1 = nibbleTMP;

      tr ^= 1;
    }

    if (nibble_0 == B00000100 && nibble_1 == B00000001) //Is a multitap
      totalUsb += 5;
  }

  //reset TH and TR to high
  setTH_High();
  
  if (portState[currentPort-1] == DB9_TR_OUTPUT)
    setTR_High();

  delayMicroseconds(4);
}

void setup() {
  int i;

  //init onboard led pin
  pinMode(LED_BUILTIN, OUTPUT);


  //Port 1 pins

  //init output pins
  pinMode(SAT1_TH, OUTPUT);
  digitalWrite(SAT1_TH, HIGH);

  //init input pins
  pinMode(SAT1_D0, INPUT_PULLUP);
  pinMode(SAT1_D1, INPUT_PULLUP);
  pinMode(SAT1_D2, INPUT_PULLUP);
  pinMode(SAT1_D3, INPUT_PULLUP);
  pinMode(SAT1_TL, INPUT_PULLUP);

  //init TR pin. Can change to input or output during execution. Defaults to input to support megadrive controllers
  pinMode(SAT1_TR, INPUT_PULLUP);


  //Port 2 pins
#if SATURN_PORTS > 1
  pinMode(SAT2_TH, OUTPUT);
  digitalWrite(SAT2_TH, HIGH);

  pinMode(SAT2_D0, INPUT_PULLUP);
  pinMode(SAT2_D1, INPUT_PULLUP);
  pinMode(SAT2_D2, INPUT_PULLUP);
  pinMode(SAT2_D3, INPUT_PULLUP);
  pinMode(SAT2_TL, INPUT_PULLUP);

  pinMode(SAT2_TR, INPUT_PULLUP);
#endif


  //hat table angles. ----RLDU
  hatTable[B00001111] = JOYSTICK_HATSWITCH_RELEASE;
  hatTable[B00001110] = 0;
  hatTable[B00000110] = 45;
  hatTable[B00000111] = 90;
  hatTable[B00000101] = 135;
  hatTable[B00001101] = 180;
  hatTable[B00001001] = 225;
  hatTable[B00001011] = 270;
  hatTable[B00001010] = 315;

  for(i = 0; i < MAX_USB_STICKS; i++)
    for(int x = 0; x < 8; x++)
      lastState[i][x] = 1;

  delayMicroseconds(10);

  //detect multitap on ports
  for (i = 1; i <= SATURN_PORTS; i++) {
    currentPort = i;
    detectMultitap();
  }

  //limit to 7 usb controllers
  if (totalUsb > MAX_USB_STICKS)
    totalUsb = MAX_USB_STICKS;

  //create usb controllers
  for (i = 0; i < totalUsb; i++) {
    usbStick[i] = new Joystick_ (
      JOYSTICK_DEFAULT_REPORT_ID + i,
      JOYSTICK_TYPE_GAMEPAD,
      10,     // buttonCount
      1,      // hatSwitchCount (0-2)
      true,   // includeXAxis
      true,   // includeYAxis
      false,  // includeZAxis
      false,  // includeRxAxis
      false,  // includeRyAxis
      false,  // includeRzAxis
      false,  // includeRudder
      true,   // includeThrottle
      false,  // includeAccelerator
      true,   // includeBrake
      false   // includeSteering
    );
  }

  //Set usb parameters and default values
  for (i = 0; i < totalUsb; i++) {
    usbStick[i]->begin(false);//disable automatic sendState
    usbStick[i]->setXAxisRange(0, 255);
    usbStick[i]->setYAxisRange(0, 255);
    usbStick[i]->setThrottleRange(0, 255);
    usbStick[i]->setBrakeRange(0, 255);

    usbStick[i]->setXAxis(128);
    usbStick[i]->setYAxis(128);
    usbStick[i]->setThrottle(0);
    usbStick[i]->setBrake(0);
    usbStick[i]->sendState();
  }
  
  delay(50);

  dstart (115200);
  //debugln (F("Powered on!"));
}

void loop() {
  start = micros();
  joyCount = 0;

  //read saturn ports
  for (int i = 1; i <= SATURN_PORTS; i++) {
    currentPort = i;
    readSatPort();
  }

  //report usb hid
  reportUsb();

  //clear state changed
  joyStateChanged = B00000000;

  //sleep if total loop time was less than sleepTime
  end = micros();
  delta = end - start;
  if (delta < sleepTime) {
    delta = sleepTime - delta;
    delayMicroseconds(delta);
    //debugln(delta);
  }

  if (joyCount == 0) { //blink led while no controller connected
    digitalWriteFast(LED_BUILTIN, HIGH);
    delay(500);
    digitalWriteFast(LED_BUILTIN, LOW);
    delay(500);
  }

}
