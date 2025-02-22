// Lock Controller code for ESP32 (Not tested yet!)
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE service and characteristic UUIDs
#define SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID "0000ffe1-0000-1000-8000-00805f9b34fb"

// Pin definitions
const int doorsRelayPin1 = 15;
const int doorsRelayPin2 = 16;
const int trunkRelayPin1 = 17;

const int motorTimeMs = 50;

bool deviceConnected = false;
bool oldDeviceConnected = false;
bool autoLocking = false;
bool doorsLocked = true;
bool isAuthenticated = false; // Track authentication state

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;

void unlockTrunk()
{
  digitalWrite(trunkRelayPin1, HIGH);
  delay(motorTimeMs);
  digitalWrite(trunkRelayPin1, LOW);
  Serial.println("ut");
}

void unlockDoors()
{
  digitalWrite(doorsRelayPin1, HIGH);
  digitalWrite(doorsRelayPin2, LOW);
  delay(motorTimeMs);
  digitalWrite(doorsRelayPin1, LOW);
  digitalWrite(doorsRelayPin2, LOW);
  Serial.println("ud");
  doorsLocked = false;
}

void lockDoors()
{
  digitalWrite(doorsRelayPin1, LOW);
  digitalWrite(doorsRelayPin2, HIGH);
  delay(motorTimeMs);
  digitalWrite(doorsRelayPin1, LOW);
  digitalWrite(doorsRelayPin2, LOW);
  Serial.println("ld");
  doorsLocked = true;
}

void checkSerial()
{
  if (Serial.available() > 0)
  {
    String data = Serial.readStringUntil('\n');
    if (data == "ld")
    {
      lockDoors();
    }
    else if (data == "ud")
    {
      unlockDoors();
    }
    else if (data == "ut")
    {
      unlockTrunk();
    }
  }
}

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    isAuthenticated = false; // Reset authentication on new connection
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
    isAuthenticated = false; // Reset authentication on disconnect
    if (autoLocking)
    {
      lockDoors();
    }
    autoLocking = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
    {
      String command = String(value.c_str());
      command.trim();

      // Handle authentication
      if (command.startsWith("AUTH:"))
      {
        String receivedPin = command.substring(5);
        if (receivedPin == LOCK_PIN)
        {
          isAuthenticated = true;
          pCharacteristic->setValue("AUTH_OK");
          pCharacteristic->notify();
        }
        else
        {
          pCharacteristic->setValue("AUTH_FAIL");
          pCharacteristic->notify();
        }
        return;
      }

      // Only process commands if authenticated
      if (!isAuthenticated)
      {
        Serial.println("Not authenticated");
        pCharacteristic->setValue("NOT_AUTH");
        pCharacteristic->notify();
        return;
      }

      // Process authenticated commands
      if (command == "ds")
      {
        pCharacteristic->setValue(doorsLocked ? "ld" : "ud");
        pCharacteristic->notify();
      }
      else if (command == "ld")
      {
        pCharacteristic->setValue("ld");
        pCharacteristic->notify();
        lockDoors();
      }
      else if (command == "ud")
      {
        pCharacteristic->setValue("ud");
        pCharacteristic->notify();
        unlockDoors();
      }
      else if (command == "ut")
      {
        unlockTrunk();
      }
      else if (command == "al")
      {
        autoLocking = true;
        if (doorsLocked)
        {
          unlockDoors();
        }
      }
      else if (command == "ald")
      {
        autoLocking = false;
      }
    }
  }
};

void setup()
{
  Serial.println("Starting BLE Lock Controller");
  // Initialize pins
  pinMode(doorsRelayPin1, OUTPUT);
  pinMode(doorsRelayPin2, OUTPUT);
  pinMode(trunkRelayPin1, OUTPUT);

  digitalWrite(doorsRelayPin1, LOW);
  digitalWrite(doorsRelayPin2, LOW);
  digitalWrite(trunkRelayPin1, LOW);

  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("ESP32_Lock");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create the BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // Set minimum connection interval to 7.5ms
  pAdvertising->setMaxPreferred(0x10); // Set maximum connection interval to 20ms
  BLEDevice::startAdvertising();

  Serial.println("BLE Lock Controller Ready");
}

void loop()
{
  checkSerial();

  // Handle BLE connection events
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);                  // Give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // Restart advertising
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected)
  {
    oldDeviceConnected = deviceConnected;
  }
}