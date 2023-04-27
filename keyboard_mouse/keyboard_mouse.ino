#include <Mouse.h>

#define RIGHT_BUTTON 4
#define LEFT_BUTTON 5
#define MIDDLE_BUTTON 6



/*
   Original code: https://github.com/felis/USB_Host_Shield_2.0/tree/master/examples/HID/USBHIDJoystick
   Modified by Nyanyan
*/

#include <usbhid.h>
#include <hiduniversal.h>
#include <usbhub.h>

// Satisfy IDE, which only needs to see the include statment in the ino.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>

#define N_OLD_DATA 5

int mouse_dx_data[N_OLD_DATA], mouse_dy_data[N_OLD_DATA];
int raw_mouse_dx, raw_mouse_dy;
int mouse_dx, mouse_dy;

class JoystickReportParser : public HIDReportParser {
  public:
    virtual void Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);
};

void JoystickReportParser::Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf) {
  raw_mouse_dx = -(int8_t)buf[2];
  raw_mouse_dy = (int8_t)buf[1];
}






USB Usb;
USBHub Hub(&Usb);
HIDUniversal Hid(&Usb);
JoystickReportParser Joy;






void setup() {
  for (int i = 0; i < N_OLD_DATA; ++i) {
    mouse_dx_data[i] = 0;
    mouse_dy_data[i] = 0;
  }
  mouse_dx = 0;
  mouse_dy = 0;
  raw_mouse_dx = 0;
  raw_mouse_dy = 0;
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

#define DELTA_WEIGHT 0.7
#define ABS_WEIGHT 0.2

void loop() {
  Usb.Task();
  
  for (int i = 1; i < N_OLD_DATA; ++i) {
    mouse_dx_data[i - 1] = mouse_dx_data[i];
    mouse_dy_data[i - 1] = mouse_dy_data[i];
  }
  mouse_dx_data[N_OLD_DATA - 1] = raw_mouse_dx;
  mouse_dy_data[N_OLD_DATA - 1] = raw_mouse_dy;

  float avg_dx = 0.0, avg_dy = 0.0;
  for (int i = 0; i < N_OLD_DATA; ++i) {
    avg_dx += mouse_dx_data[i];
    avg_dy += mouse_dy_data[i];
  }
  avg_dx /= N_OLD_DATA;
  avg_dy /= N_OLD_DATA;
  float dif_dx = fabs(raw_mouse_dx - avg_dx);
  float dif_dy = fabs(raw_mouse_dy - avg_dy);
  dif_dx *= DELTA_WEIGHT;
  dif_dy *= DELTA_WEIGHT;
  dif_dx = min(1.0, dif_dx);
  dif_dy = min(1.0, dif_dy);
  float weighted_abs_dx = fabs(raw_mouse_dx) * ABS_WEIGHT;
  float weighted_abs_dy = fabs(raw_mouse_dy) * ABS_WEIGHT;
  weighted_abs_dx = min(1.0, weighted_abs_dx);
  weighted_abs_dy = min(1.0, weighted_abs_dy);
  mouse_dx = (dif_dx * 0.3 + weighted_abs_dx * 0.7) * raw_mouse_dx * 0.8;
  mouse_dy = (dif_dy * 0.3 + weighted_abs_dy * 0.7) * raw_mouse_dy * 0.8;

  Serial.print(raw_mouse_dx);
  Serial.print('\t');
  Serial.print(raw_mouse_dy);
  Serial.print('\t');
  Serial.print('\t');
  Serial.print(dif_dx);
  Serial.print('\t');
  Serial.print(dif_dy);
  Serial.print('\t');
  Serial.print('\t');
  Serial.print(weighted_abs_dx);
  Serial.print('\t');
  Serial.print(weighted_abs_dy);
  Serial.print('\t');
  
  Serial.print('\t');
  Serial.print(mouse_dx);
  Serial.print('\t');
  Serial.print(mouse_dy);

  Serial.println("");
  
/*
  Serial.print(digitalRead(RIGHT_BUTTON));
  Serial.print(digitalRead(LEFT_BUTTON));
  Serial.print(digitalRead(MIDDLE_BUTTON));
  Serial.print("\tX: ");
  Serial.print(mouse_dx);
  Serial.print("\tY: ");
  Serial.print(mouse_dy);
  Serial.println("");
*/
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
