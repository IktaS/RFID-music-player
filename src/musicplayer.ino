#include <musicplayer.h>
#include <Wire.h>
#include <SPI.h>
#include <stdint.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>
#include <string>

#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>

MFRC522DriverPinSimple ss_pin(SS_PIN); // Configurable, see typical pin layout above.

MFRC522DriverSPI driver{ss_pin}; // Create SPI driver.

MFRC522 mfrc522{driver}; // Create MFRC522 instance.

/* Create an instance of MIFARE_Key */
MFRC522::MIFARE_Key key;

// Defining MIFARE Block Data
#define BLOCK_SIZE 16
#define DATA_LENGTH 144
#define BLOCK_COUNT DATA_LENGTH / 16

/* Set the start block to which we want to write data */
/* Be aware of Sector Trailer Blocks */
int startBlock = 4;
int endBlock = BLOCK_COUNT + (BLOCK_COUNT / 4) + 1;

/* Create another array to read data from all blocks */
/* Legthn of buffer should be 2 Bytes more than the size of Block (16 Bytes) */
byte PICCDataBufferLen = BLOCK_COUNT * BLOCK_SIZE;
byte readPICCData[BLOCK_COUNT * BLOCK_SIZE];

/* Create another array to read data from Block */
/* Legthn of buffer should be 2 Bytes more than the size of Block (16 Bytes) */
byte bufferLen = BLOCK_SIZE + 2;
byte readBuffer[BLOCK_SIZE + 2];
byte writeBuffer[BLOCK_SIZE];

MFRC522Constants::StatusCode status;

// WiFi Setup
WiFiManager wifiManager;

// MQTT Setup
WiFiClient espClient;
PubSubClient client(espClient);
const char *willMessageOffline = "off";
const char *willMessageOnline = "on";

// setup topic name
String mqttWillTopic = String(topic_prefix) + "/" + String(device_id) + "/" + String(will_topic);
String mqttPlayTopic = String(topic_prefix) + "/" + String(device_id) + "/" + String(play_topic);
#ifdef WRITE_MODE
String mqttWriteModeTopic = String(topic_prefix) + "/" + String(device_id) + "/" + String(write_mode_topic);
String mqttWriteValueTopic = String(topic_prefix) + "/" + String(device_id) + "/" + String(write_value_topic);
#endif

// Application Setup
#ifdef WRITE_MODE
boolean WriteMode;
byte WriteValue[DATA_LENGTH];
#endif

void setup_PCD()
{
  mfrc522.PCD_Init(); // Init MFRC522 board.
#ifdef DEBUG
  MFRC522Debug::PCD_DumpVersionToSerial(mfrc522, Serial); // Show details of PCD - MFRC522 Card Reader details.
#endif
  /* Prepare the ksy for authentication */
  /* All keys are set to FFFFFFFFFFFFh at chip delivery from the factory */
  for (byte i = 0; i < 6; i++)
  {
    key.keyByte[i] = 0xFF;
  }
}

void setup_wifi()
{
  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;
  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  // res = wm.autoConnect("AutoConnectAP", "password"); // password protected ap
  if (!res)
  {
    Serial.println("Failed to connect");
    ESP.restart();
  }
  else
  {
    // if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void mqttconnect()
{
  /* Loop until reconnected */
  while (!client.connected())
  {
    Serial.print("MQTT connecting ...");
    /* client ID */
    String clientId = device_id;
    /* connect now */
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password, mqttWillTopic.c_str(), 1, 1, willMessageOffline))
    {
      Serial.println("connected");
      /* subscribe topic with default QoS 0*/
      delay(100);
      client.publish(mqttPlayTopic.c_str(), willMessageOnline);
#ifdef WRITE_MODE
      client.subscribe(mqttWriteModeTopic.c_str());
      client.subscribe(mqttWriteValueTopic.c_str());
#endif
    }
    else
    {
      Serial.print("failed, status code =");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      /* Wait 5 seconds before retrying */
      delay(5000);
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  payload[length] = '\0';
  String value = String((char *)payload);
#ifdef DEBUG
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print(" , ");
  Serial.print(value);
  Serial.println("] ");
#endif

#ifdef WRITE_MODE
  if (mqttWriteModeTopic.equals(topic))
  {
#ifdef DEBUG
    Serial.println("setting write mode...");
#endif
    if (value == "on")
    {
      WriteMode = true;
    }
    else
    {
      WriteMode = false;
    }
#ifdef DEBUG
    Serial.println("success setting write mode...");
#endif
  }

  if (mqttWriteValueTopic.equals(topic))
  {
#ifdef DEBUG
    Serial.println("setting write value...");
#endif
    memset(WriteValue, 0, DATA_LENGTH);
    memcpy(WriteValue, value.c_str(), DATA_LENGTH);
#ifdef DEBUG
    Serial.println("success setting write value...");
#endif
  }
#endif
}

void setup()
{
  Serial.begin(115200); // Initialize serial communications with the PC for debugging.
  while (!Serial)
    ; // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4).

  setup_PCD();
  setup_wifi();
}

void loop()
{
  if (!client.connected())
  {
    mqttconnect();
  }
  client.loop();
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
  {
    delay(50);
    return;
  }

#ifdef DEBUG
  Serial.print("\n");
  Serial.println("**Card Detected**");
  /* Print UID of the Card */
  Serial.print(F("Card UID:"));
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.print("\n");
#endif
#ifdef WRITE_MODE
  if (WriteMode)
  {
    /* Call 'WriteDataToBlock' function, which will write data to the block */
#ifdef DEBUG
    Serial.print("\n");
    Serial.println("Writing to Data Block...");
#endif
    WriteDataToBlock(WriteValue);
#ifdef DEBUG
    Serial.println("Finished writing");
#endif
  }
  else
  {
    /* Read data from the same block */
#ifdef DEBUG
    Serial.print("\n");
    Serial.println("Reading from Data Block...");
#endif
    memset(readPICCData, 0, PICCDataBufferLen);
    ReadDataFromBlock(readPICCData);

#ifdef DEBUG
    /* Print the data read from block */
    for (int j = 0; j < PICCDataBufferLen; j++)
    {
      Serial.write(readPICCData[j]);
    }
    Serial.print("\n");
    Serial.println("Finished reading");
#endif
#endif
    char str[(PICCDataBufferLen) + 1];
    memcpy(str, readPICCData, PICCDataBufferLen);
    str[PICCDataBufferLen] = 0;

#ifdef DEBUG
    Serial.println("publishing play message...");
    Serial.println(str);
#endif
    client.publish(mqttPlayTopic.c_str(), str);
#ifdef WRITE_MODE
  }
#endif

  // halts the card and stop auth to prepare for next card;
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

#ifdef WRITE_MODE
void WriteDataToBlock(byte PICCData[])
{
  int piccDataIndex = 0;
  for (int i = startBlock; i < endBlock; i++)
  {
    int blockNum = i;
    // skip access block
    if (i % 4 == 3)
    {
      continue;
    }
    status = mfrc522.PCD_Authenticate(MFRC522Constants::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));
#ifdef DEBUG
    if (status != MFRC522Constants::STATUS_OK)
    {
      Serial.print("Authentication failed for Write: ");
      Serial.println(MFRC522Debug::GetStatusCodeName(status));
      return;
    }
    else
    {
      Serial.println("Authentication success");
    }
#endif

    memset(writeBuffer, 0, BLOCK_SIZE);
    for (int j = 0; j < BLOCK_SIZE; j++)
    {
      writeBuffer[j] = PICCData[piccDataIndex];
      piccDataIndex++;
    }

    /* Write data to the block */
    status = mfrc522.MIFARE_Write(blockNum, writeBuffer, BLOCK_SIZE);
#ifdef DEBUG
    if (status != MFRC522Constants::STATUS_OK)
    {
      Serial.print("Writing to Block failed: ");
      Serial.println(MFRC522Debug::GetStatusCodeName(status));
      return;
    }
    else
    {
      Serial.println("Data was written into Block successfully");
    }
#endif
  }
}
#endif

void ReadDataFromBlock(byte PICCData[])
{
  int piccDataIndex = 0;
  for (int i = startBlock; i < endBlock; i++)
  {
    int blockNum = i;
    // skip access block
    if (i % 4 == 3)
    {
      continue;
    }
    /* Authenticating the desired data block for Read access using Key A */
    status = mfrc522.PCD_Authenticate(MFRC522Constants::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));
#ifdef DEBUG
    if (status != MFRC522Constants::STATUS_OK)
    {
      Serial.print("Authentication failed for Read: ");
      Serial.println(MFRC522Debug::GetStatusCodeName(status));
      return;
    }
    else
    {
      Serial.println("Authentication success");
    }
#endif

    /* Reading data from the Block */
    status = mfrc522.MIFARE_Read(blockNum, readBuffer, &bufferLen);
#ifdef DEBUG
    if (status != MFRC522Constants::STATUS_OK)
    {
      Serial.print("Reading failed: ");
      Serial.println(MFRC522Debug::GetStatusCodeName(status));
      return;
    }
    else
    {
      Serial.println("Block was read successfully");
    }
#endif
    for (int j = 0; j < BLOCK_SIZE; j++)
    {
      PICCData[piccDataIndex] = readBuffer[j];
      piccDataIndex++;
    }
  }
}