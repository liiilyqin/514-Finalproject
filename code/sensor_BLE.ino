#include "driver/adc.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <stdlib.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long previousMillis = 0;
const long interval = 1000;

const float soundSpeed = 0.0343; // sound speed in cm/usec


// TODO: Change the UUID to your own (any specific one works, but make sure they're different from others'). You can generate one here: https://www.uuidgenerator.net/
#define SERVICE_UUID        "834b597d-62c0-42f2-86dd-c5bf6a433c4f"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

 
#define mic 2; // Microphone Pin (ADC Channel 6 / GPIO34)
const int sampleWin = 100; //Time window (ms)
 
int soundLevel() {
    unsigned long startMillis = millis();
    int signalMax = 0;
    int signalMin = 4095; // ESP32 ADC is 12-bit (0-4095)

    while (millis() - startMillis < sampleWin) {
        int sample = adc1_get_raw(ADC1_CHANNEL_2); // Read raw ADC value
        if (sample > signalMax) signalMax = sample;
        if (sample < signalMin) signalMin = sample;
    }
 
    return signalMax - signalMin; // Peak-to-peak amplitude
}
 
 
float dBConvert(int p2p) {
    float Vrms = (p2p / 2.0) * (3.3 / 4095.0) / sqrt(2); // Convert ADC value to voltage RMS (Estimate)
    float dB = 32+20 * log10(Vrms / 0.001); // Estimated Reference value of .001 found through trial and error. To get better result use SPL meter or sound source with known dB
    return dB;
}

void setup() {
 
    Serial.begin(115200);
    Serial.println("Starting BLE work!");
 
    adc1_config_width(ADC_WIDTH_BIT_12);    // Set ADC resolution (12-bit)
    adc1_config_channel_atten(ADC1_CHANNEL_2, ADC_ATTEN_DB_11);  // Set attenuation

    BLEDevice::init("XIAO_ESP32S3_Lily");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setValue("Hello World");
    pService->start();
    // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined! Now you can read it in your phone!");
}
 
void loop() {
    int p2p = soundLevel();
    float dB = dBConvert(p2p);
    Serial.println(dB);

    if (deviceConnected) {
        // Send new readings to database
        // TODO: change the following code to send your own readings and processed data
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        char buffer[32];
        snprintf(buffer, sizeof(buffer), "db: %.2f db", dB);
        pCharacteristic->setValue(buffer);
        pCharacteristic->notify();

        Serial.println(buffer);
        }
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);  // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising();  // advertise again
        Serial.println("Start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
    delay(1000);
}
 