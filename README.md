# SaturnMultiUSB

Convert Sega Saturn controllers to USB using an Arduino Leonardo.

Works with up to 2 controllers. And up to 7 if using one Multitap (6 Player Adaptor).

Code have some hardcoded delay between controller reads depending on what's connected:
* 1 controller: 1ms.
* 2 controllers: 2ms.
* Multitap: 4ms.

## Bonus
By using a simple adapter it also works with megadrive/genesis controllers and multitaps (Team Player MK-1647).

Controllers can be connected directly to the saturn ports or to a saturn multitap.

## Compatibility

It was tested with the following official accessories:
* Saturn Digital Pad
* Saturn 3D Analog Pad (digital and analog mode)
* Saturn Arcade Stick
* Saturn Twin Stick
* Saturn Multitap
* Megadrive 3-Button
* Megadrive 6-Button
* Megadrive 8bidto M30 2.4G
* Megadrive Multitap (MK-1647).

RetroBit's wireless controller do not work when connected directly but works when using a Multitap.

## Usage

When powering up it will scan the ports for a Multitap and create the USB joysticks.
* No Multitap: 2 USB joysticks.
* A Saturn Multitap: 7 USB joysticks.
* A Megadrive Multitap: 5 USB joysticks.

So if you want to use a multitap make sure to connect it to the arduino before powering it.

After the power-on setup you can freely hot swap controllers.

#### Button mapping for USB HID and PS3

| Saturn | HID | PS3     |
|--------|-----|---------|
| D-PAD  | HAT | D-PAD   |
| A      | 1   | X       |
| B      | 2   | O       |
| C      | 5   | R1      |
| X      | 0   | &#9723; |
| Y      | 3   | &#9651; |
| Z      | 4   | L1      |
| L      | 6   | L2      |
| R      | 7   | R2      |
| Start  | 9   | Start   |

## Connection

Attention! Port 1 pins changed on july 26 2021.<br/>
Review your wiring if you made the adapter before that date and plan to upgrade the arduino's sketch.

Saturn pins:

![pins on console](docs/pins01.jpg)
![pins on controller](docs/pins02.jpg)

#### Saturn port 1

| Saturn  | Arduino | Description |
| ------- |-------- |------------ |
| 1       | 5V      | VCC/POWER   |
| 2       | A5      | D1/DATA 1   |
| 3       | A4      | D0/DATA 0   |
| 4       | A2      | S0/SEL/TH   |
| 5       | 7       | S1/REQ/TR   |
| 6       | A1      | S2/ACK/TL   |
| 7       | A0      | D3/DATA 3   |
| 8       | A3      | D2/DATA 2   |
| 9       | GND     | GROUND      |

#### Saturn port 2

| Saturn | Arduino | Description |
| -------|-------- |------------ |
| 1      | 5V      | VCC/POWER   |
| 2      | 3       | D1/DATA 1   |
| 3      | 2       | D0/DATA 0   |
| 4      | 8       | S0/SEL/TH   |
| 5      | 9       | S1/REQ/TR   |
| 6      | 5       | S2/ACK/TL   |
| 7      | 6       | D3/DATA 3   |
| 8      | 4       | D2/DATA 2   |
| 9      | GND     | GROUND      |


##### Megadrive to saturn pin mapping

| Saturn  | Mega DB9 | Description |
| ------- |----------|------------ |
| 1       | 5        | VCC/POWER   |
| 2       | 2        | D1/DATA 1   |
| 3       | 1        | D0/DATA 0   |
| 4       | 7        | S0/SEL/TH   |
| 5       | 9        | S1/REQ/TR   |
| 6       | 6        | S2/ACK/TL   |
| 7       | 4        | D3/DATA 3   |
| 8       | 3        | D2/DATA 2   |
| 9       | 8        | GROUND      |

## Credits

My father for helping with the electronics stuff.

It uses the [ArduinoJoystickLibrary](https://github.com/MHeironimus/ArduinoJoystickLibrary)

Inspired by raphnet's [saturn_usb](https://github.com/raphnet/saturn_usb).

Darthcloud's [Evolution of SEGA's IO Interface](https://hackaday.io/project/170365-blueretro/log/180790-evolution-of-segas-io-interface-from-sg-1000-to-saturn) article was an essential read to understand the protocol.

#### Warning
Code and wiring directions are provided to you 'as is' and without any warranties. Use at your own risk.

The pinout is specific for the Arduino Leonardo. Code uses [Direct Port Manipulation](https://www.arduino.cc/en/Reference/PortManipulation).
