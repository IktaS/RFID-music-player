#include <Wire.h>
#include <SPI.h>
#include <stdint.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
// #include <MFRC522DriverI2C.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>

MFRC522DriverPinSimple ss_pin(5); // Configurable, see typical pin layout above.

MFRC522DriverSPI driver{ss_pin}; // Create SPI driver.
// MFRC522DriverI2C driver{}; // Create I2C driver.
MFRC522 mfrc522{driver}; // Create MFRC522 instance.
/* Create an instance of MIFARE_Key */
MFRC522::MIFARE_Key key;

#define BLOCK_SIZE 16
#define DATA_LENGTH 144
#define BLOCK_COUNT DATA_LENGTH / 16

/* Set the start block to which we want to write data */
/* Be aware of Sector Trailer Blocks */
int startBlock = 4;
int endBlock = BLOCK_COUNT + (BLOCK_COUNT / 4) + 1;
/* Create an array of 16 Bytes and fill it with data */
/* This is the actual data which is going to be written into the card */
byte PICCData[DATA_LENGTH] = {"https://youtube.com/playlist?list=PLJF2q_ClkUEQF8k24Ql8zC7l2O-850X_A"};

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

void setup()
{
  Serial.begin(115200); // Initialize serial communications with the PC for debugging.
  while (!Serial)
    ;                                                     // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4).
  mfrc522.PCD_Init();                                     // Init MFRC522 board.
  MFRC522Debug::PCD_DumpVersionToSerial(mfrc522, Serial); // Show details of PCD - MFRC522 Card Reader details.
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
}

void loop()
{
  /* Prepare the ksy for authentication */
  /* All keys are set to FFFFFFFFFFFFh at chip delivery from the factory */
  for (byte i = 0; i < 6; i++)
  {
    key.keyByte[i] = 0xFF;
  }
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    return;
  }

  // Select one of the cards.
  if (!mfrc522.PICC_ReadCardSerial())
  {
    return;
  };

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

  /* Call 'WriteDataToBlock' function, which will write data to the block */
  Serial.print("\n");
  Serial.println("Writing to Data Block...");
  WriteDataToBlock(PICCData);

  /* Read data from the same block */
  Serial.print("\n");
  Serial.println("Reading from Data Block...");
  ReadDataFromBlock(readPICCData);
  /* If you want to print the full memory dump, uncomment the next line */
  // mfrc522.PICC_DumpToSerial(&(mfrc522.uid));

  /* Print the data read from block */
  for (int j = 0; j < PICCDataBufferLen; j++)
  {
    Serial.write(readPICCData[j]);
  }
  Serial.print("\n");
}

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
      for (int j = 0; j < BLOCK_SIZE; j++)
      {
        PICCData[piccDataIndex] = readBuffer[j];
        piccDataIndex++;
      }
    }
  }
}