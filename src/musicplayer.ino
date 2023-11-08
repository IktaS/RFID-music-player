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
int endBlock = startBlock + BLOCK_COUNT + (BLOCK_COUNT / 4) + 1;

/* Create another array to read data from all blocks */
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
      // turn on power light if mqtt connected
      digitalWrite(PWR_PIN, HIGH);
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
      // blink power light if mqtt disconnected
      digitalWrite(PWR_PIN, LOW);
      delay(5000);
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  payload[length] = '\0';
  String value = String((char *)payload);

#ifdef WRITE_MODE
  if (mqttWriteModeTopic.equals(topic))
  {
    if (value == "on")
    {
      WriteMode = true;
      digitalWrite(WRT_PIN, HIGH);
    }
    else
    {
      WriteMode = false;
      digitalWrite(WRT_PIN, LOW);
    }
  }

  if (mqttWriteValueTopic.equals(topic))
  {
    memset(WriteValue, 0, DATA_LENGTH);
    memcpy(WriteValue, value.c_str(), DATA_LENGTH);
  }
#endif
}

void setup()
{
  Serial.begin(115200); // Initialize serial communications with the PC for debugging.
  while (!Serial)
    ; // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4).

  pinMode(PWR_PIN, OUTPUT);
  pinMode(WRT_PIN, OUTPUT);
  pinMode(BZR_PIN, OUTPUT);

  // turn on power light
  digitalWrite(PWR_PIN, HIGH);

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

#ifdef WRITE_MODE
  if (WriteMode)
  {
    digitalWrite(WRT_PIN, LOW);
    tone(BZR_PIN, BZR_TONE);
    /* Call 'WriteDataToBlock' function, which will write data to the block */
    WriteDataToBlock(WriteValue);
    digitalWrite(WRT_PIN, HIGH);
    noTone(BZR_PIN);
  }
  else
  {
#endif

    digitalWrite(WRT_PIN, HIGH);
    tone(BZR_PIN, BZR_TONE);
    /* Read data from the same block */
    ReadAndPublishFromTag();
    digitalWrite(WRT_PIN, LOW);
    noTone(BZR_PIN);

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

    memset(writeBuffer, 0, BLOCK_SIZE);
    for (int j = 0; j < BLOCK_SIZE; j++)
    {
      writeBuffer[j] = PICCData[piccDataIndex];
      piccDataIndex++;
    }

    /* Write data to the block */
    status = mfrc522.MIFARE_Write(blockNum, writeBuffer, BLOCK_SIZE);
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
  }
}
#endif

void ReadAndPublishFromTag()
{
  memset(readPICCData, 0, PICCDataBufferLen);
  ReadDataFromBlock(readPICCData);

  char str[(PICCDataBufferLen) + 1];
  memcpy(str, readPICCData, PICCDataBufferLen);
  str[PICCDataBufferLen] = 0;

  client.publish(mqttPlayTopic.c_str(), str);
}

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

    /* Reading data from the Block */
    status = mfrc522.MIFARE_Read(blockNum, readBuffer, &bufferLen);
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
    for (int j = 0; j < BLOCK_SIZE; j++)
    {
      PICCData[piccDataIndex] = readBuffer[j];
      piccDataIndex++;
    }
  }
}

void BlinkLEDFromHigh(uint8_t pin, uint8_t final)
{
  digitalWrite(pin, LOW);
  delay(100);
  digitalWrite(pin, HIGH);
  delay(100);
  digitalWrite(pin, final);
}

void BlinkLEDFromLow(uint8_t pin, uint8_t final)
{
  digitalWrite(pin, HIGH);
  delay(100);
  digitalWrite(pin, LOW);
  delay(100);
  digitalWrite(pin, final);
}