#include <Arduino.h>
#include <BLEDevice.h>

#define SERVICE_UUID        "d6b366cb-796f-48d3-a9f2-25a4cc4e8dda"
#define CHARACTERISTIC_UUID "59547458-5840-4e82-bb4a-3f2a4d8b6f10"

#define LED_PIN 21

// stepper
#define AIN1 D0
#define AIN2 D1
#define BIN1 D2
#define BIN2 D3


BLEAddress *serverAddress;
BLERemoteCharacteristic* remoteCharacteristic;
BLEClient* client;

bool doConnect = false;
bool connected = false;


/* -----------------------------
   电压参数 (可校准)
----------------------------- */

float NO_CUP_VOLTAGE = 3.30;
float EMPTY_CUP_VOLTAGE = 2.50;

// 电压 -> 水量 (可修改)
float voltageTable[] = {2.50, 2.30, 2.10, 1.90, 1.70};
float volumeTable[]  = {0,    100,  200,  300,  400};  // ml

int tableSize = 5;


/* -----------------------------
   状态变量
----------------------------- */

float lastStableVoltage = 3.3;
float currentVoltage = 3.3;

unsigned long lastDrinkTime = 0;

bool cupPresent = false;

#define STABLE_THRESHOLD 0.03
#define STABLE_TIME 1500

unsigned long stableTimer = 0;
float lastVoltage = 3.3;


/* -----------------------------
   电机控制
----------------------------- */

int currentStep = 0;

int stepSequence[4][4] = {
  {1,0,1,0},
  {0,1,1,0},
  {0,1,0,1},
  {1,0,0,1}
};


void setStep(int stepIndex){
  digitalWrite(AIN1, stepSequence[stepIndex][0]);
  digitalWrite(AIN2, stepSequence[stepIndex][1]);
  digitalWrite(BIN1, stepSequence[stepIndex][2]);
  digitalWrite(BIN2, stepSequence[stepIndex][3]);
}


void stepMotor(int steps, int direction){

  for(int i=0;i<steps;i++){

    currentStep += direction;

    if(currentStep > 3) currentStep = 0;
    if(currentStep < 0) currentStep = 3;

    setStep(currentStep);

    delayMicroseconds(2000);
  }
}


/* -----------------------------
   LED控制
----------------------------- */

void blinkLED(){

    digitalWrite(LED_PIN,HIGH);
    delay(200);

    digitalWrite(LED_PIN,LOW);
    delay(200);
}


/* -----------------------------
   Debug打印
----------------------------- */

void debugPrint(String msg){

    Serial.print("[DEBUG] ");
    Serial.println(msg);
}


/* -----------------------------
   电压 -> 水量
----------------------------- */

float voltageToVolume(float v){

    for(int i=0;i<tableSize-1;i++){

        if(v <= voltageTable[i] && v > voltageTable[i+1]){

            float ratio = (v-voltageTable[i+1]) /
                          (voltageTable[i]-voltageTable[i+1]);

            return volumeTable[i+1] +
                   ratio*(volumeTable[i]-volumeTable[i+1]);
        }
    }

    return volumeTable[tableSize-1];
}


/* -----------------------------
   水量 -> 电机角度
----------------------------- */

int volumeToSteps(float volume){

    float maxVolume = 400;

    float ratio = volume / maxVolume;

    int maxSteps = 600;

    return ratio * maxSteps;
}


void updatePointer(float voltage){

    float volume = voltageToVolume(voltage);

    int targetSteps = volumeToSteps(volume);

    debugPrint("Volume: " + String(volume));

    stepMotor(targetSteps,1);
}


/* -----------------------------
   状态检测
----------------------------- */

void detectCupEvent(float newVoltage){

    if(lastStableVoltage <= EMPTY_CUP_VOLTAGE &&
       newVoltage <= EMPTY_CUP_VOLTAGE){

        if(newVoltage > lastStableVoltage){

            debugPrint("User drank water");

            lastDrinkTime = millis();
        }
    }

    else if(newVoltage > EMPTY_CUP_VOLTAGE){

        debugPrint("Cup removed");

        cupPresent = false;
    }

    else if(newVoltage <= EMPTY_CUP_VOLTAGE){

        debugPrint("Cup placed");

        cupPresent = true;

        updatePointer(newVoltage);

        lastDrinkTime = millis();
    }

    lastStableVoltage = newVoltage;
}


/* -----------------------------
   稳定检测
----------------------------- */

void processVoltage(float v){

    currentVoltage = v;

    if(abs(v - lastVoltage) < STABLE_THRESHOLD){

        if(millis() - stableTimer > STABLE_TIME){

            detectCupEvent(v);

            stableTimer = millis();
        }

    }else{

        stableTimer = millis();
    }

    lastVoltage = v;
}


/* -----------------------------
   BLE 回调
----------------------------- */

void notifyCallback(
    BLERemoteCharacteristic*,
    uint8_t* data,
    size_t length,
    bool){

    String str="";

    for(int i=0;i<length;i++)
        str += (char)data[i];

    float voltage = str.toFloat();

    Serial.print("Voltage: ");
    Serial.println(voltage);

    processVoltage(voltage);
}


/* -----------------------------
   BLE扫描
----------------------------- */

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {

    void onResult(BLEAdvertisedDevice advertisedDevice){

        if (advertisedDevice.haveServiceUUID() &&
            advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {

            Serial.println("Found Server!");

            serverAddress = new BLEAddress(advertisedDevice.getAddress());

            doConnect = true;

            BLEDevice::getScan()->stop();
        }
    }
};


bool connectToServer(){

    client = BLEDevice::createClient();

    if(!client->connect(*serverAddress)){
        Serial.println("Connection failed");
        return false;
    }

    BLERemoteService* remoteService =
        client->getService(SERVICE_UUID);

    if(remoteService == nullptr){
        Serial.println("Service not found");
        return false;
    }

    remoteCharacteristic =
        remoteService->getCharacteristic(CHARACTERISTIC_UUID);

    if(remoteCharacteristic->canNotify())
        remoteCharacteristic->registerForNotify(notifyCallback);

    connected = true;

    return true;
}


/* -----------------------------
   Setup
----------------------------- */

void setup(){

    Serial.begin(115200);

    pinMode(LED_PIN,OUTPUT);

    pinMode(AIN1,OUTPUT);
    pinMode(AIN2,OUTPUT);
    pinMode(BIN1,OUTPUT);
    pinMode(BIN2,OUTPUT);

    BLEDevice::init("");

    BLEScan* scan = BLEDevice::getScan();

    scan->setAdvertisedDeviceCallbacks(
        new MyAdvertisedDeviceCallbacks());

    scan->setActiveScan(true);

    scan->start(5,false);
}


/* -----------------------------
   Loop
----------------------------- */

void loop(){

    if(doConnect && !connected){

        if(connectToServer())
            Serial.println("Connection success");

        doConnect = false;
    }

    // 5秒未喝水提醒
    if(cupPresent){

        if(millis() - lastDrinkTime > 5000){

            blinkLED();
        }
    }

    delay(50);
}