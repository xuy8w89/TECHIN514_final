#include <Arduino.h>

// 控制H桥输入
#define AIN1 D0
#define AIN2 D1
#define BIN1 D2
#define BIN2 D3

int currentStep = 0;

// 全步驱动序列（双极）
int stepSequence[4][4] = {
  {1, 0, 1, 0},
  {0, 1, 1, 0},
  {0, 1, 0, 1},
  {1, 0, 0, 1}
};

void setStep(int stepIndex) {
  digitalWrite(AIN1, stepSequence[stepIndex][0]);
  digitalWrite(AIN2, stepSequence[stepIndex][1]);
  digitalWrite(BIN1, stepSequence[stepIndex][2]);
  digitalWrite(BIN2, stepSequence[stepIndex][3]);
}

void stepMotor(int steps, int direction) {
  for (int i = 0; i < steps; i++) {
    currentStep += direction;

    if (currentStep > 3) currentStep = 0;
    if (currentStep < 0) currentStep = 3;

    setStep(currentStep);
    delayMicroseconds(2000);  // 调速
  }
}

void setup() {
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
}

void loop() {
  stepMotor(600, 1);   // 顺时针
  delay(1000);
  stepMotor(600, -1);  // 逆时针
  delay(1000);
}