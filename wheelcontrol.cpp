#include <Arduino.h>

// Motor A (Left)
const int EnableL = 25;
const int HighL = 26;
const int LowL = 27;

// Motor B (Right)
const int EnableR = 14;
const int HighR = 12;
const int LowR = 13;

// Input from Pi
const int D0 = 4;
const int D1 = 16;
const int D2 = 17;
const int D3 = 5;

int a, b, c, d, data;

void Data() {
  a = digitalRead(D0);
  b = digitalRead(D1);
  c = digitalRead(D2);
  d = digitalRead(D3);
  data = 8 * d + 4 * c + 2 * b + a;
  Serial.print("Data: ");
  Serial.println(data);
}

void setup() {
  Serial.begin(115200);

  // Motor direction control pins
  pinMode(HighL, OUTPUT);
  pinMode(LowL, OUTPUT);
  pinMode(HighR, OUTPUT);
  pinMode(LowR, OUTPUT);

  // PWM pins (Enable pins)
  pinMode(EnableL, OUTPUT);
  pinMode(EnableR, OUTPUT);

  // Input pins from Raspberry Pi
  pinMode(D0, INPUT_PULLUP);
  pinMode(D1, INPUT_PULLUP);
  pinMode(D2, INPUT_PULLUP);
  pinMode(D3, INPUT_PULLUP);
}

void setMotorSpeed(int leftSpeed, int rightSpeed) {
  analogWrite(EnableL, leftSpeed); // 0–255
  analogWrite(EnableR, rightSpeed);
}

// Movement functions
void Forward() {
  digitalWrite(HighL, LOW);
  digitalWrite(LowL, HIGH);
  digitalWrite(HighR, LOW);
  digitalWrite(LowR, HIGH);
  setMotorSpeed(255, 255);
}

void Backward() {
  digitalWrite(HighL, HIGH);
  digitalWrite(LowL, LOW);
  digitalWrite(HighR, HIGH);
  digitalWrite(LowR, LOW);
  setMotorSpeed(255, 255);
}

void Stop() {
  setMotorSpeed(0, 0);
}

void Left1()  { setMotorSpeed(200, 255); }
void Left2()  { setMotorSpeed(160, 255); }
void Left3()  { setMotorSpeed(100, 255); }
void Right1() { setMotorSpeed(255, 200); }
void Right2() { setMotorSpeed(255, 160); }
void Right3() { setMotorSpeed(255, 100); }

void loop() {
  Data();

  // Always enable forward direction logic by default
  digitalWrite(HighL, LOW);
  digitalWrite(LowL, HIGH);
  digitalWrite(HighR, LOW);
  digitalWrite(LowR, HIGH);

  switch (data) {
    case 0:  Serial.println("Moving Forward"); Forward(); break;
    case 1:  Serial.println("Moving Right1");  Right1();  break;
    case 2:  Serial.println("Moving Right2");  Right2();  break;
    case 3:  Serial.println("Moving Right3");  Right3();  break;
    case 4:  Serial.println("Moving Left1");   Left1();   break;
    case 5:  Serial.println("Moving Left2");   Left2();   break;
    case 6:  Serial.println("Moving Left3");   Left3();   break;
    default: Serial.println("Stopping");       Stop();    break;
  }

  delay(100);  // Prevent serial spamming
}