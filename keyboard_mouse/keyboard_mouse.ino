#include <Mouse.h>

#define RIGHT_BUTTON 14
#define LEFT_BUTTON 15
#define MIDDLE_BUTTON 16

#define DELAY_MOUSE 12
#define DELAY_WHEEL 70

int modified_x_center, modified_y_center, pre_x, pre_y;
bool last_small_x = false, last_small_y = false;

void setup() {
  pinMode(X_PIN, INPUT);
  pinMode(Y_PIN, INPUT);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);
  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(MIDDLE_BUTTON, INPUT_PULLUP);
  modified_x_center = analogRead(X_PIN);
  modified_y_center = analogRead(Y_PIN);
  pre_x = modified_x_center;
  pre_y = modified_y_center;
  Mouse.begin();
  Serial.begin(115200);
}

bool mouse_is_stop() {
  int x = analogRead(X_PIN);
  int y = analogRead(Y_PIN);
  return 
    modified_x_center - MOUSE_MARGIN <= x && x <= modified_x_center + MOUSE_MARGIN && 
    modified_y_center - MOUSE_MARGIN <= y && y <= modified_y_center + MOUSE_MARGIN;
}

signed char calc_mouse_amount(int val, int center, int sign) {
  long amount = 0;
  int sign2 = 0;
  if (val < center) {
    amount = center - MOUSE_MARGIN - val;
    sign2 = -1;
  } else {
    amount = val - center - MOUSE_MARGIN;
    sign2 = 1;
  }
  long res = amount * 0.1 * amount * 0.1 * MOUSE_SPEED;
  res = min(res, MOUSE_MAX);
  return res * sign * sign2;
}

signed char calc_wheel_amount(int val, int center, int sign) {
  int amount = 0;
  int sign2 = 0;
  if (val < center) {
    amount = center - MOUSE_MARGIN - val;
    sign2 = 1;
  } else {
    amount = val - center - MOUSE_MARGIN;
    sign2 = -1;
  }
  return amount * sign * sign2 * WHEEL_SPEED;
}

void loop() {
  Serial.print(digitalRead(RIGHT_BUTTON));
  Serial.print(digitalRead(LEFT_BUTTON));
  Serial.print(digitalRead(MIDDLE_BUTTON));
  Serial.print("\t");
  Serial.print(analogRead(X_PIN));
  Serial.print("\t");
  Serial.print(analogRead(Y_PIN));
  Serial.print("\t");
  Serial.print(mouse_is_stop());
  Serial.print("\t");
  Serial.print(modified_x_center);
  Serial.print("\t");
  Serial.print(modified_y_center);
  Serial.print("\t");
  Serial.print(last_small_x);
  Serial.print(last_small_y);
  Serial.print("\t");
  Serial.print(calc_mouse_amount(analogRead(X_PIN), modified_x_center, X_SIGN));
  Serial.print("\t");
  Serial.print(calc_mouse_amount(analogRead(Y_PIN), modified_y_center, Y_SIGN));
  Serial.print("\t");
  Serial.print(calc_wheel_amount(analogRead(Y_PIN), modified_y_center, Y_SIGN));
  Serial.print("\n");

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

  //if (mouse_is_stop()){
  //  if (digitalRead(MIDDLE_BUTTON)) {
  //    Mouse.release(MOUSE_MIDDLE);
  //  } else {
  //    Mouse.press(MOUSE_MIDDLE);
  //  }
  //} else {
  int x = analogRead(X_PIN);
  int y = analogRead(Y_PIN);

  if (abs(x - modified_x_center) > MOUSE_MARGIN) {
    bool n_last_small_x = x < modified_x_center;
    if (last_small_x && !n_last_small_x) {
      modified_x_center = X_CENTER + MOUSE_ERROR;
    } else if (!last_small_x && n_last_small_x) {
      modified_x_center = X_CENTER - MOUSE_ERROR;
    }
    last_small_x = n_last_small_x;
  }

  if (abs(y - modified_y_center) > MOUSE_MARGIN) {
    bool n_last_small_y = y < modified_y_center;
    if (last_small_y && !n_last_small_y) {
      modified_y_center = Y_CENTER + MOUSE_ERROR;
    } else if (!last_small_y && n_last_small_y) {
      modified_y_center = Y_CENTER - MOUSE_ERROR;
    }
    last_small_y = n_last_small_y;
  }

  if (!digitalRead(MIDDLE_BUTTON)) {
    Mouse.move(0, 0, calc_wheel_amount(y, modified_y_center, Y_SIGN));
    delay(DELAY_WHEEL);
  } else {
    Mouse.move(calc_mouse_amount(x, modified_x_center, X_SIGN), calc_mouse_amount(y, modified_y_center, Y_SIGN), 0);
    delay(DELAY_MOUSE);
  }
  //}
}
