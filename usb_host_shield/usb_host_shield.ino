#include <usbhid.h>
#include <hiduniversal.h>
#include <usbhub.h>

// Satisfy IDE, which only needs to see the include statment in the ino.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>






int mouse_dx, mouse_dy;

struct GamePadEventData {
  uint8_t X, Y, Z1, Z2, Rz;
};

class JoystickEvents {
  public:
    virtual void OnGamePadChanged(const GamePadEventData *evt);
    virtual void OnHatSwitch(uint8_t hat);
    virtual void OnButtonUp(uint8_t but_id);
    virtual void OnButtonDn(uint8_t but_id);
};

#define RPT_GEMEPAD_LEN    5

class JoystickReportParser : public HIDReportParser {
    JoystickEvents *joyEvents;

    uint8_t oldPad[RPT_GEMEPAD_LEN];
    uint8_t oldHat;
    uint16_t oldButtons;

  public:
    JoystickReportParser(JoystickEvents *evt);

    virtual void Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);
};

JoystickReportParser::JoystickReportParser(JoystickEvents *evt) :
  joyEvents(evt),
  oldHat(0xDE),
  oldButtons(0) {
  for (uint8_t i = 0; i < RPT_GEMEPAD_LEN; i++)
    oldPad[i] = 0xD;
}

void JoystickReportParser::Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf) {
  bool match = true;

  // Checking if there are changes in report since the method was last called
  for (uint8_t i = 0; i < RPT_GEMEPAD_LEN; i++)
    if (buf[i] != oldPad[i]) {
      match = false;
      break;
    }

  // Calling Game Pad event handler
  if (!match && joyEvents) {
    joyEvents->OnGamePadChanged((const GamePadEventData*)buf);

    for (uint8_t i = 0; i < RPT_GEMEPAD_LEN; i++) oldPad[i] = buf[i];
  }

  uint8_t hat = (buf[5] & 0xF);

  // Calling Hat Switch event handler
  if (hat != oldHat && joyEvents) {
    joyEvents->OnHatSwitch(hat);
    oldHat = hat;
  }

  uint16_t buttons = (0x0000 | buf[6]);
  buttons <<= 4;
  buttons |= (buf[5] >> 4);
  uint16_t changes = (buttons ^ oldButtons);

  // Calling Button Event Handler for every button changed
  if (changes) {
    for (uint8_t i = 0; i < 0x0C; i++) {
      uint16_t mask = (0x0001 << i);

      if (((mask & changes) > 0) && joyEvents) {
        if ((buttons & mask) > 0)
          joyEvents->OnButtonDn(i + 1);
        else
          joyEvents->OnButtonUp(i + 1);
      }
    }
    oldButtons = buttons;
  }
}

void JoystickEvents::OnGamePadChanged(const GamePadEventData *evt) {
  mouse_dx = -(int8_t)evt->Z1;
  mouse_dy = (int8_t)evt->Y;
  Serial.print("X: ");
  Serial.print(mouse_dx);
  Serial.print("\tY: ");
  Serial.print(mouse_dy);
  Serial.println("");
}

void JoystickEvents::OnHatSwitch(uint8_t hat) {
  Serial.print("Hat Switch: ");
  PrintHex<uint8_t > (hat, 0x80);
  Serial.println("");
}

void JoystickEvents::OnButtonUp(uint8_t but_id) {
  Serial.print("Up: ");
  Serial.println(but_id, DEC);
}

void JoystickEvents::OnButtonDn(uint8_t but_id) {
  Serial.print("Dn: ");
  Serial.println(but_id, DEC);
}






USB Usb;
USBHub Hub(&Usb);
HIDUniversal Hid(&Usb);
JoystickEvents JoyEvents;
JoystickReportParser Joy(&JoyEvents);

void setup() {
  mouse_dx = 0;
  mouse_dy = 0;
  Serial.begin(115200);
#if !defined(__MIPSEL__)
  while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
#endif
  Serial.println("Start");

  if (Usb.Init() == -1)
    Serial.println("OSC did not start.");

  delay(200);

  if (!Hid.SetReportParser(0, &Joy))
    ErrorMessage<uint8_t > (PSTR("SetReportParser"), 1);
}

void loop() {
  Usb.Task();
}
