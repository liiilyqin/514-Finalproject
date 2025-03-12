#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
// Client Code
#include "BLEDevice.h"
//#include "BLEScan.h"

// TODO: change the service UUID to the one you are using on the server side.
// The remote service we wish to connect to.
static BLEUUID serviceUUID("834b597d-62c0-42f2-86dd-c5bf6a433c4f");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

// 定义步进电机控制引脚
#define COIL_A1 D0
#define COIL_A2 D1
#define COIL_B1 D2
#define COIL_B2 D3

// 定义步进电机步进序列（全步模式）
const int stepSequence[4][4] = {
    {1, 0, 1, 0}, // Step 1
    {0, 1, 1, 0}, // Step 2
    {0, 1, 0, 1}, // Step 3
    {1, 0, 0, 1}  // Step 4
};

// 步进电机角度转换（200步 = 360°）
#define STEPS_PER_REV 200
#define DEGREE_TO_STEP(deg) ((deg) * STEPS_PER_REV / 360)

// 定义分贝范围对应的角度
#define NORMAL_ANGLE 0
#define HIGHER_ANGLE 90
#define DANGEROUS_ANGLE 180

// 当前步进电机角度
int currentAngle = 0;

// TODO: define new global variables for data collection
std::vector<float> distanceData;
 
// TODO: define a new function for data aggregation
void processData(float distance) {
    distanceData.push_back(distance);
    if (distanceData.size() > 10) { // Limit to last 10 measurements
        distanceData.erase(distanceData.begin());
    }
    Serial.print("Collected ");
    Serial.print(distanceData.size());
    Serial.println(" data points.");
}

//
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
 
    // Convert received data to a string
    String receivedString = String((char*)pData).substring(0, length);
 
    Serial.print("Raw BLE Data: ");
    Serial.println(receivedString);
 
    // Extract numeric distance from formatted string
    float receivedDistance = 0;
    int index = receivedString.indexOf(": ");
    if (index != -1) {
        receivedDistance = receivedString.substring(index + 2).toFloat();
    } else {
        Serial.println("Error: Unexpected BLE data format.");
        return;
    }
 
    // Print current, max, and min distances
    Serial.print("Extracted db: ");
    Serial.print(receivedDistance);
    Serial.print(" db");
  
    processData(receivedDistance);
}

// BLEClient* pClient;
String serverDeviceName = "";
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    serverDeviceName = myDevice->getName();
    Serial.print("✅ Successfully connected to BLE server: ");
    Serial.println(serverDeviceName.c_str());
  }
 
  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
 
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");
 
    pClient->setClientCallbacks(new MyClientCallback());
 
    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");
    pClient->setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)
 
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");
    serverDeviceName = myDevice->getName();
    Serial.print("✅ Successfully connected to BLE server: ");
    Serial.println(serverDeviceName.c_str());
 
    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");
 
    // Read the value of the characteristic.
    if(pRemoteCharacteristic->canRead()) {
      std::string value = std::string(pRemoteCharacteristic->readValue().c_str());
      Serial.print("The characteristic value was: ");
      Serial.println(value.c_str());
    }
 
    if(pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallback);
 
    connected = true;
    return true;
}

// /**
//  * Scan for BLE servers and find the first one that advertises the service we are looking for.
//  */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
 
    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
 
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
 
    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks
// 步进电机旋转到指定角度
void rotateToAngle(int targetAngle, int stepDelay) {
    int targetStep = DEGREE_TO_STEP(targetAngle);
    int currentStep = DEGREE_TO_STEP(currentAngle);
    int stepDiff = targetStep - currentStep;
    
    if (stepDiff > 0) {
        stepMotor(stepDiff, false, stepDelay);  // 正向旋转
    } else if (stepDiff < 0) {
        stepMotor(-stepDiff, true, stepDelay); // 反向旋转
    }
    
    currentAngle = targetAngle;  // 更新当前角度
}

// 步进电机步进控制
void stepMotor(int steps, bool reverse, int stepDelay) {
    for (int i = 0; i < steps; i++) {
        for (int step = 0; step < 4; step++) {
            int s = reverse ? (3 - step) : step;
            digitalWrite(COIL_A1, stepSequence[s][0]);
            digitalWrite(COIL_A2, stepSequence[s][1]);
            digitalWrite(COIL_B1, stepSequence[s][2]);
            digitalWrite(COIL_B2, stepSequence[s][3]);
            delay(stepDelay);
        }
    }
}


#define LED_PIN 10  // LED 连接到 D10
#define BUTTON_PIN 9 // 按钮连接到 D9

void setup() {
    Serial.begin(115200);
    pinMode(COIL_A1, OUTPUT);
    pinMode(COIL_A2, OUTPUT);
    pinMode(COIL_B1, OUTPUT);
    pinMode(COIL_B2, OUTPUT);

    Serial.println("Starting Arduino BLE Client application...");
    BLEDevice::init("");
  
    // Retrieve a Scanner and set the callback we want to use to be informed when we
    // have detected a new device.  Specify that we want active scanning and start the
    // scan to run for 5 seconds.
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);

    //components
    pinMode(LED_PIN, OUTPUT);      // LED 设为输出模式
    pinMode(BUTTON_PIN, INPUT_PULLUP); // 按钮设为输入模式，并启用上拉电阻
    digitalWrite(LED_PIN, HIGH);   // LED 默认常亮
}

bool ledShouldBeOn = false; // 记录 LED 是否应该亮

void loop() {
    // If the flag "doConnect" is true then we have scanned for and found the desired
    // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
    // connected we set the connected flag to be true.
    if (doConnect == true) {
      if (connectToServer()) {
        Serial.println("We are now connected to the BLE Server.");
      } else {
        Serial.println("We have failed to connect to the server; there is nothin more we will do.");
      }
      doConnect = false;
    }
  
    // If we are connected to a peer BLE Server, update the characteristic each time we are reached
    // with the current time since boot.
    if (connected) {
      String newValue = "Time since boot: " + String(millis()/1000);
      Serial.println("Setting new characteristic value to \"" + newValue  + "\"");
  
      // Set the characteristic's value to be the array of bytes that is actually a string.
      pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
    }else if(doScan){
      BLEDevice::getScan()->start(0);  // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
    }
  
    
    
    int buttonState = digitalRead(BUTTON_PIN); // 读取按钮状态

    if (buttonState == LOW) { // 如果按钮被按下（低电平）
        ledShouldBeOn = false;
        digitalWrite(LED_PIN, LOW);  // 关闭 LED
    } 

    if (!distanceData.empty()) {
        float latestDb = distanceData.back(); // 获取最新的 db 值

        // 判断分贝范围并调整步进电机角度
        if (latestDb <= 40) {
            rotateToAngle(NORMAL_ANGLE, 10);
        } else if (latestDb > 40 && latestDb <= 50) {
            rotateToAngle(HIGHER_ANGLE, 10);
        } else {
            rotateToAngle(DANGEROUS_ANGLE, 10);
            ledShouldBeOn = true;
        }

        // 串口输出分贝值
        Serial.print("Decibel: ");
        Serial.print(latestDb);
        Serial.println(" dB");
        delay(1000); // 每2秒更新一次
    }

    // 根据 ledShouldBeOn 变量控制 LED
    if (ledShouldBeOn) {
        digitalWrite(LED_PIN, HIGH);
    }
}
