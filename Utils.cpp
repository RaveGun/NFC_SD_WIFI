/**************************************************************************
    class Utils: Some small functions.
**************************************************************************/

#include "Config.h"
#include <SD.h>
#include "PN532.h"
#include "Utils.h"
#include "UserManager.h"
#include "Graphics.h"
#include <Stream.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WString.h>

const char baseC[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

SSD1306 display(OLED_ADDR, OLED_SDA, OLED_SCL);
File root;

/* WIFI AP settings */
const char *ssid = "NFCoffee";
const char *password = "freecoffee";
uint16_t totFiles = 0;
char androidDate[9];
WiFiServer server(31415);
WiFiClient serverClient;

void OLEDScreen::Initialize(void)
{
	// Setup OLED
    display.init();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
//    display.setFont(ArialMT_Plain_10);
//    display.invertDisplay();
//    display.flipScreenVertically();
}

void OLEDScreen::ShowReady(void)
{
	display.clear();
	display.drawXbm(43, 31, coffee_oled_width, coffee_oled_height, coffee_oled_bits);
	display.display();
}

void OLEDScreen::ShowSDError(void)
{
	display.clear();
	display.drawXbm(27, 0, sderror_width, sderror_height, sderror_bits);
	display.display();
}

void OLEDScreen::ShowWiFi(void)
{
	display.clear();
	display.drawXbm(23, 0, wifi_oled_width, wifi_oled_height, wifi_oled_bits);
	display.display();
}

void OLEDScreen::ShowDT(void)
{
	display.clear();
	display.drawXbm(11, 0, transfer_oled_width, transfer_oled_height, transfer_oled_bits);
	display.display();
}

void OLEDScreen::ShowBackup(void)
{
	display.clear();
	display.drawXbm(32, 0, backup_oled_width, backup_oled_height, backup_oled_bits);
	display.display();
}

void OLEDScreen::ShowNFCRF(void)
{
	static bool state = true;

	if(true == state)
	{
		/* Display image */
		display.drawXbm(47, 0, nfc_rf_sign_width, nfc_rf_sign_height, nfc_rf_signs_bits);
		display.display();
		state = false;
	}
	else
	{
		display.setColor(BLACK);
		display.fillRect(47, 0, nfc_rf_sign_width, nfc_rf_sign_height);
		display.setColor(WHITE);
		display.display();
		state = true;
	}
}

void OLEDScreen::ShowProgressBar(uint16_t currentVal, uint16_t totalVal)
{
	display.setColor(BLACK);
	display.fillRect(13, 39, 102, 11);
	display.setColor(WHITE);
	display.drawProgressBar(14, 40, 100, 8, (currentVal * 100 / totalVal));
	display.display();
}

void WLAN::ZeroInit(void)
{
	WiFi.disconnect(false);
	WiFi.mode(WIFI_OFF);
}

void WLAN::Initialize(void)
{
	WiFi.mode(WIFI_AP);
	WiFi.softAP(ssid, password);
    server.begin();
    server.setNoDelay(false);
    //Utils::Print("TCP Server Setup done", LF);
}

bool WLAN::StartTCP(void)
{
#define TOTAL_CONNECTION_TIMEOUT (0xCAFEUL)
	uint32_t lTimeout = TOTAL_CONNECTION_TIMEOUT;

	while(lTimeout)
	{
		if (server.hasClient())
		{
		    //Utils::Print("Client connected");
		    break;
		}
		else
		{
			// No connection show progress bar
			display.setColor(BLACK);
			display.fillRect(13, 39, 102, 11);
			display.setColor(WHITE);
			display.drawProgressBar(14, 40, 100, 8, (lTimeout * 100 / TOTAL_CONNECTION_TIMEOUT));
			display.display();
			lTimeout--;
		}
	}
	if(0 == lTimeout)
		return(false);
	else
		return(true);
}

bool WLAN::StartTransffer(void)
{
	bool retVal = true;

	if (!serverClient || !serverClient.connected())
	{
		if (serverClient) serverClient.stop();
		serverClient = server.available();
		//Utils::Print("New client\n");

		/* Wait for secret Key */
		if(true == WLAN::ReadSecretKey(androidDate))
		{
			//Utils::Print(androidDate, LF);
			root.rewindDirectory();
			totFiles = Utils::getNumFiles(root);
			if(0 != totFiles)
			{
				root.rewindDirectory();
				if(false == Utils::printDirectory(root, totFiles))
				{
					/* Transfer failure */
					retVal = false;
				}
			}
			else
			{
				/* There is nothing to backup */
				retVal = false;
			}
		}
		else
		{
			/* Wrong key or no key has been received */
			retVal = false;
		}
		if (serverClient) serverClient.stop();
	}
	return (retVal);
}

bool WLAN::ReadSecretKey(char RcvDate[9])
{
	bool retVal = true;
	uint8_t secretKey[8];

	/* A secret key has to be passed and the current device date */
	serverClient.setTimeout(1000);
	if(8u != serverClient.readBytes(secretKey, 8))
	{
		/* Timeout or wrong stream length */
		retVal = false;
	}
	else
	{
		/* Check for valid key */
		uint16_t chkSum1 = ((uint16_t)secretKey[0] << 8) | (uint16_t)secretKey[1];
		uint16_t chkSum2 = ((uint16_t)secretKey[7] << 8) | (uint16_t)secretKey[6];
		uint16_t year = ((uint16_t)secretKey[2] << 8) | (uint16_t)secretKey[3];
		uint8_t month = secretKey[4];
		uint8_t day = secretKey[5];
		if((chkSum1 == chkSum2) && ((year + (uint16_t)month + (uint16_t)day) == chkSum1))
		{
			/* correct key - setup return data */
			/* Stub */
			//Utils::PrintHexBuf(secretKey, 8u, LF);
		    char lBuf[9];
			sprintf(lBuf, "%04d%02d%02d", year, month, day);
			memcpy(RcvDate, lBuf, 9u);
		}
		else
		{
			retVal = false;
		}
	}

	return (retVal);
}

bool Utils::Backup_Data(void)
{
	bool retVal = true;
	bool skipFolderCreation = false;
	File inputFile;
    File outputFile;
	File dir;
	uint16_t fileIdx = 0;

    uint8_t copyBuf[4];


	char newFolderName[1+8+1+3];
	sprintf(newFolderName, "/%s.BKP", androidDate);

	/*root.rewindDirectory();
	dir = root;*/

	/* Create folder if not already exists */
	if(!SD.exists(newFolderName))
	{
		if(SD.mkdir(newFolderName))
		{
			/* Folder creation was successful */
		} else {
			/* Error creating folder */
			/* ToDo: Damn, move to SD error */
			retVal = false;
		}
	}
	if(true == retVal)
	{
		/* no error creating the folder or folder already there */
		/* now, copy files */
		root.rewindDirectory();
		dir = root;

		/* we need to copy from allFiles to androidDate.bkp/allFiles */
		while (inputFile = dir.openNextFile(FILE_READ), inputFile)
		{
		  if (!inputFile.isDirectory())
		  {
			  fileIdx++;
			  OLEDScreen::ShowProgressBar(fileIdx, totFiles);
			  //Utils::Print(inputFile.name(), LF);

			  char copyFileFullPath[1+8+1+3+1+8+1+3+1];
			  sprintf(copyFileFullPath, "%s/%s", newFolderName, inputFile.name());
			  //Utils::Print(copyFileFullPath, 0);

			  outputFile = SD.open(copyFileFullPath, FILE_WRITE);
			  if(outputFile)
			  {
				  /* copy data */
				  while (inputFile.available())
				  {
					  outputFile.write(inputFile.read());
				  }
				  //Utils::Print(".. saved. ", LF);
				  outputFile.close();
			  }

			  /* delete original */
			  if(!SD.remove(inputFile.name()))
			  {
				  /* Error deleting */
#ifdef STD_PRINT_EN
				  Utils::Print("error deleting: ", 0);
				  Utils::Print(inputFile.name(), LF);
#endif
				  retVal = false;
			  }
		  }
		  inputFile.close();
		}
	}
	/* create backup folder if no already existing */
	/* move all files from / to androidDate.bkp */
	/* return true if anything went well */
	return (retVal);
}

void Utils::Base36(uint64_t u64_ID, char *s8_LF)
{
  unsigned char pos = 11;
  while (u64_ID)
  {
    s8_LF[pos] = baseC[u64_ID % 36];
    u64_ID /= 36;
    pos--;
    if(8 == pos) pos--;
  }
}

#ifdef STD_PRINT_EN

void Utils::Print(const char* s8_Text, const char* s8_LF) //=NULL
{
    SerialClass::Print(s8_Text);
    if (s8_LF) 
        SerialClass::Print(s8_LF);
}
void Utils::PrintDec(int s32_Data, const char* s8_LF) // =NULL
{
    char s8_Buf[20];
    sprintf(s8_Buf, "%d", s32_Data);
    Print(s8_Buf, s8_LF);
}
void Utils::PrintHex8(byte u8_Data, const char* s8_LF) // =NULL
{
    char s8_Buf[20];
    sprintf(s8_Buf, "%02X", u8_Data);
    Print(s8_Buf, s8_LF);
}
void Utils::PrintHex16(uint16_t u16_Data, const char* s8_LF) // =NULL
{
    char s8_Buf[20];
    sprintf(s8_Buf, "%04X", u16_Data);
    Print(s8_Buf, s8_LF);
}
void Utils::PrintHex32(uint32_t u32_Data, const char* s8_LF) // =NULL
{
    char s8_Buf[20];
    sprintf(s8_Buf, "%08X", (unsigned int)u32_Data);
    Print(s8_Buf, s8_LF);
}

void Utils::PrintHexBuf(const byte* u8_Data, const uint32_t u32_DataLen, const char* s8_LF, int s32_Brace1, int s32_Brace2)
{
    for (uint32_t i=0; i < u32_DataLen; i++)
    {
        if ((int)i == s32_Brace1)
            Print(" <");
        else if ((int)i == s32_Brace2)
            Print("> ");
        else if (i > 0)
            Print(" ");
        
        PrintHex8(u8_Data[i]);
    }
    if (s8_LF) Print(s8_LF);
}

// Converts an interval in milliseconds into days, hours, minutes and prints it
void Utils::PrintInterval(uint64_t u64_Time, const char* s8_LF)
{
    char Buf[30];
    u64_Time /= 60*1000;
    int s32_Min  = (int)(u64_Time % 60);
    u64_Time /= 60;
    int s32_Hour = (int)(u64_Time % 24);    
    u64_Time /= 24;
    int s32_Days = (int)u64_Time;    
    sprintf(Buf, "%d days, %02d:%02d hours", s32_Days, s32_Hour, s32_Min);
    Print(Buf, s8_LF);   
}

#endif

void Utils::GetHexBuf(const byte* u8_Data, const uint32_t u32_DataLen, String* retVal)
{
  *retVal = "";

  for (uint32_t i=0; i < u32_DataLen; i++)
  {
    *retVal += String(u8_Data[i]);
  }
  *retVal += '\0';
}


// We need a special time counter that does not roll over after 49 days (as millis() does) 
uint64_t Utils::GetMillis64()
{
    static uint32_t u32_High = 0;
    static uint32_t u32_Last = 0;

    uint32_t u32_Now = GetMillis(); // starts at zero after CPU reset

    // Check for roll-over
    if (u32_Now < u32_Last) u32_High ++;
    u32_Last = u32_Now;

    uint64_t u64_Time = u32_High;
    u64_Time <<= 32;
    u64_Time |= u32_Now;
    return u64_Time;
}

// Multi byte XOR operation In -> Out
// If u8_Out and u8_In are the same buffer use the other function below.
void Utils::XorDataBlock(byte* u8_Out, const byte* u8_In, const byte* u8_Xor, int s32_Length)
{
    for (int B=0; B<s32_Length; B++)
    {
        u8_Out[B] = u8_In[B] ^ u8_Xor[B];
    }
}

// Multi byte XOR operation in the same buffer
void Utils::XorDataBlock(byte* u8_Data, const byte* u8_Xor, int s32_Length)
{
    for (int B=0; B<s32_Length; B++)
    {
        u8_Data[B] ^= u8_Xor[B];
    }
}

// Rotate a block of 8 byte to the left by one byte.
// ATTENTION: u8_Out and u8_In must not be the same buffer!
void Utils::RotateBlockLeft(byte* u8_Out, const byte* u8_In, int s32_Length)
{
    int s32_Last = s32_Length -1;
    memcpy(u8_Out, u8_In + 1, s32_Last);
    u8_Out[s32_Last] = u8_In[0];
}

// Logical Bit Shift Left. Shift MSB out, and place a 0 at LSB position
void Utils::BitShiftLeft(uint8_t* u8_Data, int s32_Length)
{
    for (int n=0; n<s32_Length-1; n++) 
    {
        u8_Data[n] = (u8_Data[n] << 1) | (u8_Data[n+1] >> 7);
    }
    u8_Data[s32_Length - 1] <<= 1;
}

// Generate multi byte random
void Utils::GenerateRandom(byte* u8_Random, int s32_Length)
{
    uint32_t u32_Now = GetMillis();
    for (int i=0; i<s32_Length; i++)
    {
        u8_Random[i] = (byte)u32_Now;
        u32_Now *= 127773;
        u32_Now += 16807;
    }
}

// ITU-V.41 (ISO 14443A)
// This CRC is used only for legacy authentication. (not implemented anymore)
uint16_t Utils::CalcCrc16(const byte* u8_Data, int s32_Length)
{
    uint16_t u16_Crc = 0x6363;
    for (int i=0; i<s32_Length; i++)
    {
        byte ch = u8_Data[i];
        ch = ch ^ (byte)u16_Crc;
        ch = ch ^ (ch << 4);
        u16_Crc = (u16_Crc >> 8) ^ ((uint16_t)ch << 8) ^ ((uint16_t)ch << 3) ^ ((uint16_t)ch >> 4);
    }
    return u16_Crc;
}

// This CRC is used for ISO and AES authentication.
// The new Desfire EV1 authentication calculates the CRC32 also over the command, but the command is not encrypted later.
// This function allows to include the command into the calculation without the need to add the command to the same buffer that is later encrypted.
uint32_t Utils::CalcCrc32(const byte* u8_Data1, int s32_Length1, // data to process
                          const byte* u8_Data2, int s32_Length2) // optional additional data to process (these parameters may be omitted)
{
    uint32_t u32_Crc = 0xFFFFFFFF;
    u32_Crc = CalcCrc32(u8_Data1, s32_Length1, u32_Crc);
    u32_Crc = CalcCrc32(u8_Data2, s32_Length2, u32_Crc);
    return u32_Crc;
}

// private
uint32_t Utils::CalcCrc32(const byte* u8_Data, int s32_Length, uint32_t u32_Crc)
{
    for (int i=0; i<s32_Length; i++)
    {
        u32_Crc ^= u8_Data[i];
        for (int b=0; b<8; b++)
        {
            bool b_Bit = (u32_Crc & 0x01) > 0;
            u32_Crc >>= 1;
            if (b_Bit) u32_Crc ^= 0xEDB88320;
        }
    }
    return u32_Crc;
}

void Utils::GetSDCounterForCard(char* fileName, uint16_t * u16_noOfCoffees)
{
    File dataFile;
    uint8_t bufCoffee[4] = {0,0,255,255};

	dataFile = SD.open(fileName);
	if(dataFile) {
    	while (dataFile.available()) {
	    	dataFile.readBytes(&bufCoffee[0], 4u);
	    }
	    dataFile.close();
	}
	uint16_t noOfCoffees = (uint16_t)((uint16_t)bufCoffee[0] << 8 | (uint16_t)bufCoffee[1]);
	uint16_t noOfCoffeesInv = (uint16_t)((uint16_t)bufCoffee[2] << 8 | (uint16_t)bufCoffee[3]);

	if(noOfCoffees == (uint16_t)~noOfCoffeesInv) {
		*u16_noOfCoffees = noOfCoffees;
	} else {
		*u16_noOfCoffees = 1337;
	}
}

bool Utils::UpdateSDCardCounter(uint64_t u64_ID, kCard* pk_Card, uint64_t u64_StartTick)
{
    kUser k_User;
    File dataFile;
    char cardIDString[] = "00000000.000";
    uint8_t bufCoffee[4] = {0,0,255,255};
    bool retResult = true;

    Utils::Base36(u64_ID, cardIDString);
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 1, cardIDString);
//    display.display();

#if true
	dataFile = SD.open(cardIDString);
	if(dataFile) {
    	while (dataFile.available()) {
	    	dataFile.readBytes(&bufCoffee[0], 4u);
	    }
	    dataFile.close();
	}
	uint16_t noOfCoffees = (uint16_t)((uint16_t)bufCoffee[0] << 8 | (uint16_t)bufCoffee[1]);
	uint16_t noOfCoffeesInv = (uint16_t)((uint16_t)bufCoffee[2] << 8 | (uint16_t)bufCoffee[3]);

	String sCoffees = "000000";

	if(noOfCoffees == (uint16_t)~noOfCoffeesInv) {
		/* number of coffees is correct */
		noOfCoffees++;
		noOfCoffeesInv = (uint16_t)~noOfCoffees;
		bufCoffee[0] = (uint8_t)((noOfCoffees >> 8) & 0xFFu);
		bufCoffee[1] = (uint8_t)(noOfCoffees & 0xFFu);
		bufCoffee[2] = (uint8_t)((noOfCoffeesInv >> 8) & 0xFFu);
		bufCoffee[3] = (uint8_t)(noOfCoffeesInv & 0xFFu);

	    char tmpBuf[16];
	    sprintf(tmpBuf, "%d", noOfCoffees);

	    display.setFont(ArialMT_Plain_24);
		display.drawString(64, 15, tmpBuf);
		display.display();
	} else {
		/* invalid number */
		/* start from zero? */
	}

	dataFile = SD.open(cardIDString, FILE_WRITE);
    display.setFont(ArialMT_Plain_10);
	if (dataFile) {
		dataFile.seek(0u);
		dataFile.write(&bufCoffee[0], 4u);
		dataFile.close();

		display.drawString(64, 40, "Saved!");
		display.display();
	}
	else {
		retResult = false;

		display.drawString(64, 40, "ERROR!");
		display.display();
	}
#endif
	return (retResult);
}


uint16_t Utils::getNumFiles(File dir)
{
	File entry;
	uint8_t fileCount = 0;

	while (entry = dir.openNextFile(), entry)
	{
	  if (!entry.isDirectory())
	  {
		fileCount++;
	  }
	  entry.close();
	}
	return(fileCount);
}

bool Utils::printDirectory(File dir, uint16_t fileCount)
{
	File entry;
	uint16_t fileIdx = 0;
	uint16_t Coffees = 0;
    char lBuf[12+1+5];


	while(entry = dir.openNextFile(), entry) {
		if (!entry.isDirectory()) {
			//		skip DIRs
			fileIdx++;
			OLEDScreen::ShowProgressBar(fileIdx, fileCount);
			Utils::GetSDCounterForCard(entry.name(), &Coffees);
			sprintf(lBuf, "%s,%d", entry.name(), Coffees);
			serverClient.println(lBuf);
		}
		entry.close();
	}
	return (true);
}
