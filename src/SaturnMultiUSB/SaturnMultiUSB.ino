/*******************************************************************************
 * Sega Saturn controllers to USB using an Arduino Leonardo.
 * 
 * Works with digital pad and analog pad.
 * 
 * By using the multitap it's possible to connect up to 7 controllers.
 * 
 * Button mapping follows the PS3 standard.
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

//Saturn port 1
#define SAT1_TH A2
#define SAT1_TR 7
#define SAT1_D0 A5
#define SAT1_D1 A4
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

//#define ENABLE_SERIAL_DEBUG

#define MAX_USB_STICKS 7

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

short totalUsb = 2;
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

char readNibble() {
  //Read from port 1 or port 2
  uint8_t nibble = currentPort == 1 ? SAT1_PINGROUP : SAT2_PINGROUP;
  return ((nibble >> 4) & B00001000) + // D3
         ((nibble >> 2) & B00000100) + // D2
         ((nibble << 1) & B00000010) + // D1
         ((nibble >> 1) & B00000001);  // D0
}

void readSatPort() {
  uint8_t nibble_0;
  uint8_t nibble_1;

  setTH_High();
  setTR_High();
  delayMicroseconds(4);

  //if (digitalRead(SAT1_TL) == LOW) //Device is not ready
    //return;

  nibble_0 = readNibble();

  if ((nibble_0 & B00000111) == B00000100) {
    //debugln (F("DIGITAL-PAD"));
    readDigitalPad(nibble_0);
  } else {
    setTH_Low();
    delayMicroseconds(4);

    nibble_1 = readNibble();

    if (nibble_0 == B00000001 && nibble_1 == B00000001) {
      //debugln (F("3-WIRE-HANDSHAKE"));
      readThreeWire();
    }
  }

  setTH_High();
  setTR_High();
  delayMicroseconds(100);
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
  }
  else if (nibble_0 == B00000000 && nibble_1 == B00000010) {
    //debugln (F("DIGITAL"));
    readThreeWireController(false);
  }
  else if (nibble_0 == B00000001 && nibble_1 == B00000110) {
    //debugln (F("ANALOG"));
    readThreeWireController(true);
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
    usbStick[joyIndex]->setButton(8, !(nibbleTMP & B00001000)); //S
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

void readDigitalPad(uint8_t firstNibble) {
  byte joyIndex = joyCount++;
  uint8_t nibbleTMP;
  //uint8_t currentHatState;

  // L100
  //debugln (firstNibble, BIN);
  setUsbValues(joyIndex, false, 3, firstNibble);

  // RLDU
  setTH_Low();
  setTR_High();
  delayMicroseconds(4);
  nibbleTMP = readNibble();
  //debugln (nibbleTMP, BIN);
  setUsbValues(joyIndex, false, 0, nibbleTMP);

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

  r = waitTL(1); //Device is not ready yet
  if (r)
    return;

  nibble_0 = readNibble();

  if (nibble_0 == B00000001) {
    setTH_Low();
    delayMicroseconds(4);

    nibble_1 = readNibble();

    if (nibble_1 == B00000001) { //3-WIRE-HANDSHAKE
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
  }

  //reset TH and TR to high
  setTH_High();
  setTR_High();
  delayMicroseconds(4);
}


void setup() {
  int i;

  //init onboard led pin
  pinMode(LED_BUILTIN, OUTPUT);
  
  //init output pins
  pinMode(SAT1_TH, OUTPUT);
  pinMode(SAT1_TR, OUTPUT);
  digitalWrite(SAT1_TH, HIGH);
  digitalWrite(SAT1_TR, HIGH);

  pinMode(SAT2_TH, OUTPUT);
  pinMode(SAT2_TR, OUTPUT);
  digitalWrite(SAT2_TH, HIGH);
  digitalWrite(SAT2_TR, HIGH);

  //init input pins
  pinMode(SAT1_D0, INPUT_PULLUP);
  pinMode(SAT1_D1, INPUT_PULLUP);
  pinMode(SAT1_D2, INPUT_PULLUP);
  pinMode(SAT1_D3, INPUT_PULLUP);
  pinMode(SAT1_TL, INPUT_PULLUP);

  pinMode(SAT2_D0, INPUT_PULLUP);
  pinMode(SAT2_D1, INPUT_PULLUP);
  pinMode(SAT2_D2, INPUT_PULLUP);
  pinMode(SAT2_D3, INPUT_PULLUP);
  pinMode(SAT2_TL, INPUT_PULLUP);

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

  currentPort = 1;
  detectMultitap();

  currentPort = 2;
  detectMultitap();

  //limit to 7 usb controllers
  if (totalUsb > MAX_USB_STICKS)
    totalUsb = MAX_USB_STICKS;

  //create usb controllers
  for (i = 0; i < totalUsb; i++) {
    usbStick[i] = new Joystick_ (
      JOYSTICK_DEFAULT_REPORT_ID + i,
      JOYSTICK_TYPE_GAMEPAD,
      9,      // buttonCount
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

  dstart (115200);
  debugln (F("Powered on!"));
}

void loop() {
  start = micros();
  joyCount = 0;

  //read first saturn port
  currentPort = 1;
  readSatPort();

  //read second saturn port
  currentPort = 2;
  readSatPort();

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
