#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "d6b366cb-796f-48d3-a9f2-25a4cc4e8dda"
#define CHARACTERISTIC_UUID "59547458-5840-4e82-bb4a-3f2a4d8b6f10"

#define ADC_PIN D0

// 已知电阻
const float R_FIXED = 10000000.0; // 10M ohm

// ESP32C3 ADC参数
const float VREF = 3.3;
const int ADC_RESOLUTION = 4095;

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

// adaptive parameters
float lastVoltage = 0.0;
unsigned long lastSendTime = 0;
unsigned long sendInterval = 100;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Client connected");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Client disconnected");
    }
};

void setup() {

    Serial.begin(115200);

    analogReadResolution(12);
    analogSetPinAttenuation(ADC_PIN, ADC_11db);

    Serial.println("Pressure Sensor Start");

    BLEDevice::init("XIAO-ESP32C3-Server");

    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );

    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();

    Serial.println("BLE Server started");
}

void loop() {

    if(deviceConnected) {

        int adcValue = analogRead(ADC_PIN);
        float voltage = ((float)adcValue / ADC_RESOLUTION) * VREF;

        float diff = fabs(voltage - lastVoltage);

        // adaptive interval
        if(diff > 0.05) {
            sendInterval = 100;
        }
        else if(diff > 0.01) {
            sendInterval = 300;
        }
        else {
            sendInterval = 1000;
        }

        unsigned long now = millis();

        if(now - lastSendTime > sendInterval) {

            char buffer[16];
            dtostrf(voltage, 4, 3, buffer);

            pCharacteristic->setValue(buffer);
            pCharacteristic->notify();

            Serial.print("Voltage: ");
            Serial.print(buffer);
            Serial.print(" | interval: ");
            Serial.println(sendInterval);

            lastSendTime = now;
            lastVoltage = voltage;
        }
    }

}