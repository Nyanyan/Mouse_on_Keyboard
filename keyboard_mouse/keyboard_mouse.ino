#include <Mouse.h>

#define RIGHT_BUTTON 4
#define LEFT_BUTTON 5
#define MIDDLE_BUTTON 6



/*
 * Original code: https://github.com/felis/USB_Host_Shield_2.0/tree/master/examples/HID/USBHIDJoystick
 * Modified by Nyanyan
 */

#include <usbhid.h>
#include <hiduniversal.h>
#include <usbhub.h>

// Satisfy IDE, which only needs to see the include statment in the ino.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>


int mouse_dx, mouse_dy;

class JoystickReportParser : public HIDReportParser {
  public:
    virtual void Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);
};

void JoystickReportParser::Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf) {
  bool match = true;

  mouse_dx = -(int8_t)buf[2];
  mouse_dy = (int8_t)buf[1];

  Serial.print('\t');
  Serial.print(mouse_dx);
  Serial.print('\t');
  Serial.print(mouse_dy);
  
  Serial.println("");
}






USB Usb;
USBHub Hub(&Usb);
HIDUniversal Hid(&Usb);
JoystickReportParser Joy;






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

  pinMode(RIGHT_BUTTON, INPUT_PULLUP);
  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(MIDDLE_BUTTON, INPUT_PULLUP);
  
  Mouse.begin();
}

void loop() {
  Usb.Task();

  Serial.print(digitalRead(RIGHT_BUTTON));
  Serial.print(digitalRead(LEFT_BUTTON));
  Serial.print(digitalRead(MIDDLE_BUTTON));
  Serial.print("\tX: ");
  Serial.print(mouse_dx);
  Serial.print("\tY: ");
  Serial.print(mouse_dy);
  Serial.println("");
  
  if (digitalRead(RIGHT_BUTTON)) {
    Mouse.release(MOUSE_RIGHT);
  } else {
    Mouse.press(MOUSE_RIGHT);
  }

  if (digitalRead(LEFT_BUTTON)) {
    Mouse.release(MOUSE_LEFT);
  } else {
    Mouse.press(MOUSE_LEFT);
  }

  if (!digitalRead(MIDDLE_BUTTON)) {
    Mouse.move(0, 0, mouse_dy);
  } else {
    Mouse.move(mouse_dx, mouse_dy, 0);
  }
}
