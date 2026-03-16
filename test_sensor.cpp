#include <Arduino.h>

#define ADC_PIN D0

// 已知电阻
const float R_FIXED = 10000000.0; // 10M ohm

// ESP32C3 ADC参数
const float VREF = 3.3;
const int ADC_RESOLUTION = 4095;

void setup() {

    Serial.begin(115200);

    // 设置ADC分辨率
    analogReadResolution(12);

    // 设置ADC衰减 (允许读取0-3.3V)
    analogSetPinAttenuation(ADC_PIN, ADC_11db);

    Serial.println("Pressure Sensor Start");
}

void loop() {

    int adcValue = analogRead(ADC_PIN);

    // 转换为电压
    float voltage = ((float)adcValue / ADC_RESOLUTION) * VREF;

    Serial.print("ADC: ");
    Serial.print(adcValue);

    Serial.print("  Voltage: ");
    Serial.print(voltage, 3);
    Serial.print(" V");
    Serial.println();
    delay(100);
}