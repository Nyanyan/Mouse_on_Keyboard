/*
   Original code: https://github.com/felis/USB_Host_Shield_2.0/tree/master/examples/HID/USBHIDJoystick
   Modified by Nyanyan
*/

#if !defined(USBCON)
#error "This sketch needs a board with native USB (ATmega32u4: Pro Micro, Leonardo, Micro). Arduino Pro / Pro Mini (ATmega328P) cannot act as a USB mouse."
#endif

#include <Mouse.h>

#include <usbhid.h>
#include <hiduniversal.h>
#include <usbhub.h>

// Satisfy IDE, which only needs to see the include statment in the ino.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>

#define RIGHT_BUTTON 3
#define LEFT_BUTTON 2
#define MIDDLE_BUTTON 4

// After a button changes, ignore further changes for this time to filter out contact bounce
#define DEBOUNCE_MS 10

// The sensor is mounted rotated, so map its axes to the screen axes
// (current setting: screen x = -sensor y, screen y = sensor x)
#define SWAP_XY 1
#define INVERT_X 1
#define INVERT_Y 0

// Minimum interval between reports to the PC (1 or more). Sending a report on every loop
// floods remote desktop software such as Chrome Remote Desktop and makes the pointer lag.
#define SEND_INTERVAL_MS 8

// Pointer acceleration. The gain (screen pixels per sensor count) grows linearly
// from POINTER_SPEED * SLOW_RATIO at SPEED_LOW to POINTER_SPEED at SPEED_HIGH.
// Speeds are in sensor counts per second.
#define POINTER_SPEED 4.0
#define SLOW_RATIO 0.4
#define SPEED_LOW 250.0
#define SPEED_HIGH 1250.0

// Per-direction correction of the sensor movement (screen directions).
// Use these when the pointer moves faster in one direction than in the opposite one.
// Moving the finger back and forth over the same span, this sensor reported about 2.2 times
// as many counts to the left as to the right, so left is scaled down and right is scaled up.
#define SCALE_LEFT 0.67
#define SCALE_RIGHT 1.5
#define SCALE_UP 1.0
#define SCALE_DOWN 1.0

// Wheel notches per sensor count while the middle button is held
#define WHEEL_SPEED 0.15

#if SEND_INTERVAL_MS < 1
#error "SEND_INTERVAL_MS must be 1 or more"
#endif

// Sensor movement received since the last flush
long sensor_dx = 0, sensor_dy = 0;

// Fractions of a pixel / notch not sent yet
float pointer_rem_x = 0.0, pointer_rem_y = 0.0, wheel_rem = 0.0;

unsigned long last_flush_ms = 0;
bool scroll_mode = false;

class SensorReportParser : public HIDReportParser {
  public:
    virtual void Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);
};

void SensorReportParser::Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf) {
  // The sensor reports its X and Y movement in buf[1] and buf[2] (checked with usb_host_shield.ino)
  if (len < 3) {
    return;
  }
  int x = (int8_t)buf[1];
  int y = (int8_t)buf[2];
#if SWAP_XY
  int tmp = x;
  x = y;
  y = tmp;
#endif
#if INVERT_X
  x = -x;
#endif
#if INVERT_Y
  y = -y;
#endif
  // Accumulate instead of overwriting so that each report is used exactly once
  sensor_dx += x;
  sensor_dy += y;
}

USB Usb;
USBHub Hub(&Usb);
HIDUniversal Hid(&Usb);
SensorReportParser Parser;

struct Button {
  uint8_t pin;
  bool pressed;
  unsigned long changed_ms;
};

Button left_button = {LEFT_BUTTON, false, 0};
Button right_button = {RIGHT_BUTTON, false, 0};
Button middle_button = {MIDDLE_BUTTON, false, 0};

// Returns true when the state of the button changes
bool update_button(Button &button, unsigned long now) {
  bool pressed = !digitalRead(button.pin);
  if (pressed == button.pressed || now - button.changed_ms < DEBOUNCE_MS) {
    return false;
  }
  button.pressed = pressed;
  button.changed_ms = now;
  return true;
}

void send_button(uint8_t mouse_button, bool pressed) {
  if (pressed) {
    Mouse.press(mouse_button);
  } else {
    Mouse.release(mouse_button);
  }
}

// Takes the whole part out of rem, up to what fits in one report
signed char take_whole(float &rem) {
  long whole = lround(rem);
  whole = constrain(whole, -127, 127);
  rem -= whole;
  return whole;
}

float pointer_gain(float speed) {
  float t = (speed - SPEED_LOW) / (SPEED_HIGH - SPEED_LOW);
  t = constrain(t, 0.0, 1.0);
  return POINTER_SPEED * (SLOW_RATIO + (1.0 - SLOW_RATIO) * t);
}

// Converts the sensor movement received since the last flush and sends it to the PC
void flush_movement(unsigned long now) {
  long dx = sensor_dx, dy = sensor_dy;
  sensor_dx = 0;
  sensor_dy = 0;
  // Send nothing while there is no movement
  if (dx == 0 && dy == 0) {
    return;
  }

  if (scroll_mode) {
    wheel_rem -= dy * WHEEL_SPEED;
  } else {
    // Correct the sensor first so that the acceleration below also sees the corrected speed
    float fx = dx * (dx < 0 ? SCALE_LEFT : SCALE_RIGHT);
    float fy = dy * (dy < 0 ? SCALE_UP : SCALE_DOWN);
    // The time the movement took. Right after an idle period, assume two intervals.
    unsigned long dt = now - last_flush_ms;
    dt = constrain(dt, SEND_INTERVAL_MS, 2 * SEND_INTERVAL_MS);
    // Use the speed of the whole vector so that diagonal movement keeps its direction
    float speed = sqrt(fx * fx + fy * fy) * 1000.0 / dt;
    float gain = pointer_gain(speed);
    pointer_rem_x += fx * gain;
    pointer_rem_y += fy * gain;
  }
  last_flush_ms = now;

  // Movement too large for one report is split into several, so nothing is left to lag behind
  bool full;
  do {
    signed char x = take_whole(pointer_rem_x);
    signed char y = take_whole(pointer_rem_y);
    signed char wheel = take_whole(wheel_rem);
    if (x != 0 || y != 0 || wheel != 0) {
      Mouse.move(x, y, wheel);
    }
    full = abs(x) == 127 || abs(y) == 127 || abs(wheel) == 127;
  } while (full);
}

void setup() {
  Mouse.begin();
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);
  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(MIDDLE_BUTTON, INPUT_PULLUP);

  Usb.Init();

  delay(500);

  if (!Hid.SetReportParser(0, &Parser))
    ErrorMessage<uint8_t > (PSTR("SetReportParser"), 1);
}

void loop() {
  Usb.Task();

  unsigned long now = millis();
  bool left_changed = update_button(left_button, now);
  bool right_changed = update_button(right_button, now);
  bool middle_changed = update_button(middle_button, now);

  // On a button change, first send the movement made before it so that the click lands in place
  if (left_changed || right_changed || middle_changed || now - last_flush_ms >= SEND_INTERVAL_MS) {
    flush_movement(now);
  }

  if (left_changed) {
    send_button(MOUSE_LEFT, left_button.pressed);
  }
  if (right_changed) {
    send_button(MOUSE_RIGHT, right_button.pressed);
  }
  if (middle_changed) {
    scroll_mode = middle_button.pressed;
    pointer_rem_x = 0.0;
    pointer_rem_y = 0.0;
    wheel_rem = 0.0;
  }
}
