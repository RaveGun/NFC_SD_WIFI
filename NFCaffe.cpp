/**************************************************************************

  @author   DG
  NFC for Coffee

**************************************************************************/

#include "Config.h"
#include "NFCaffe.h"

#include <Arduino.h>
#include <OLEDDisplay.h>
#include <OLEDDisplayFonts.h>
#include <pins_arduino.h>
#include <SD.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "UserManager.h"
#include "Utils.h"

// This is the most important switch: It defines if you want to use Mifare Classic or Desfire EV1 cards.
// If you set this define to false the users will only be identified by the UID of a Mifare Classic or Desfire card.
// This mode is only for testing if you have no Desfire cards available.
// Mifare Classic cards have been cracked due to a badly implemented encryption.
// It is easy to clone a Mifare Classic card (including it's UID).
// You should use Defire EV1 cards for any serious door access system.
// When using Desfire EV1 cards a 16 byte data block is stored in the card's EEPROM memory that can only be read with the application master key.
// To clone a Desfire card it would be necessary to crack a 168 bit 3K3DES or a 128 bit AES key data which is impossible.
// If the Desfire card does not contain the correct data the door will not open even if the UID is correct.
// IMPORTANT: After changing this compiler switch, please execute the CLEAR command!
#define USE_DESFIRE   false

// This compiler switch defines if you use AES (128 bit) or DES (168 bit) for the PICC master key and the application master key.
// Cryptographers say that AES is better.
// But the disadvantage of AES encryption is that it increases the power consumption of the card more than DES.
// The maximum read distance is 5,3 cm when using 3DES keys and 4,0 cm when using AES keys.
// (When USE_DESFIRE == false the same Desfire card allows a distance of 6,3 cm.)
// If the card is too far away from the antenna you get a timeout error at the moment when the Authenticate command is executed.
// IMPORTANT: Before changing this compiler switch, please execute the RESTORE command on all personalized cards!
#define USE_AES   false

// This Arduino / Teensy pin is connected to the PN532 RSTPDN pin (reset the PN532)
// When a communication error with the PN532 is detected the board is reset automatically.
#define RESET_PIN         16
// The software SPI SCK  pin (Clock)
#define SPI_CLK_PIN       14
// The software SPI MISO pin (Master In, Slave Out)
#define SPI_MISO_PIN      12
// The software SPI MOSI pin (Master Out, Slave In)
#define SPI_MOSI_PIN      13
// The software SPI SSEL pin (Chip Select)
#define SPI_CS_PIN        0


// Built in LED of Wemos Mini D1
#define LED_BUILTIN 2

// SD card SPI CS pin
#define SD_CHIP_SELECT      15

#define LED_OFF (0u)

typedef enum {
	CARD_READ,
	UPLOAD_DATA,
	BACKUP_DATA,
	SDCARD_ERROR
} SM_t;

// ######################################################################################

PN532      gi_PN532;

// global variables
char		gs8_CommandBuffer[500];  // Stores commands typed by the user via Terminal and the password
uint32_t	gu32_CommandPos = 0;     // Index in gs8_CommandBuffer
uint64_t	gu64_LastPasswd = 0;     // Timestamp when the user has enetered the password successfully
uint64_t	gu64_LastID     = 0;     // The last card UID that has been read by the RFID reader
bool		gb_InitSuccess  = false; // true if the PN532 has been initialized successfully
SM_t		gSMCurrentState = CARD_READ;
kUser		k_User;
kCard		k_Card;

void SetLED(uint8_t e_LED)
{
#ifdef NO_BUZZER
    switch (e_LED)
    {
        case LED_BUILTIN:
            Utils::WritePin(LED_BUILTIN, LOW);
            break;
        default:
            Utils::WritePin(LED_BUILTIN, HIGH);
            break;
    }
#else
    switch (e_LED)
    {
        case LED_BUILTIN:
            Utils::WritePin(LED_BUILTIN, HIGH);
            break;
        default:
            Utils::WritePin(LED_BUILTIN, LOW);
            break;
    }
#endif
}

// If everything works correctly, the green LED will flash shortly (20 ms).
// If the LED does not flash permanently this means that there is a severe error.
// Additionally the LED will flash long (for 1 second) when the door is opened.
// -----------------------------------------------------------------------------
// The red LED shows a communication error with the PN532 (flash very slow),
// or someone not authorized trying to open the door (flash for 1 second)
// or on power failure the red LED flashes shortly.
void FlashLED(uint8_t e_LED, int s32_Interval)
{
#ifdef NO_BUZZER
    SetLED(e_LED);
#endif
    LongDelay(s32_Interval);
#ifdef NO_BUZZER
    SetLED(LED_OFF);
#endif
}


// Reset the PN532 chip and initialize, set gb_InitSuccess = true on success
void InitReader(bool b_ShowError)
{
    if (b_ShowError)
    {
        //SetLED(LED_BUILTIN);
#ifdef STD_PRINT_EN
        Utils::Print("Communication Error -> Reset PN532\r\n");
#endif
    }

    do // pseudo loop (just used for aborting with break;)
    {
        gb_InitSuccess = false;

        // Reset the PN532
        gi_PN532.begin(); // delay > 400 ms
		OLEDScreen::ShowReady();
		OLEDScreen::ShowNFCRF();


        byte IC, VersionHi, VersionLo, Flags;
        if (!gi_PN532.GetFirmwareVersion(&IC, &VersionHi, &VersionLo, &Flags))
            break;

#ifdef STD_PRINT_EN
        char Buf[80];
        sprintf(Buf, "Chip: PN5%02X, Firmware version: %d.%d\r\n", IC, VersionHi, VersionLo);
        Utils::Print(Buf);
        sprintf(Buf, "Supports ISO 14443A:%s, ISO 14443B:%s, ISO 18092:%s\r\n", (Flags & 1) ? "Yes" : "No",
                                                                                (Flags & 2) ? "Yes" : "No",
                                                                                (Flags & 4) ? "Yes" : "No");
        Utils::Print(Buf);
#endif

        // Set the max number of retry attempts to read from a card.
        // This prevents us from waiting forever for a card, which is the default behaviour of the PN532.
        if (!gi_PN532.SetPassiveActivationRetries())
            break;

        // configure the PN532 to read RFID tags
        if (!gi_PN532.SamConfig())
            break;

        gb_InitSuccess = true;
    }
    while (false);

    if (b_ShowError)
    {
        //LongDelay(250); // a long interval to make the LED flash very slowly
        //SetLED(LED_OFF);
    }
}


void setup()
{
    gs8_CommandBuffer[0] = 0;

    Utils::SetPinMode(LED_BUILTIN,   OUTPUT);
    FlashLED(LED_BUILTIN, 500);

#ifdef STD_PRINT_EN
    // Open USB serial port
    SerialClass::Begin(115200);
#endif

    WLAN::ZeroInit();

    OLEDScreen::Initialize();

    if (!SD.begin(SD_CHIP_SELECT)) {
    	OLEDScreen::ShowSDError();
    	gSMCurrentState = SDCARD_ERROR;
    } else {
		OLEDScreen::ShowReady();
		OLEDScreen::ShowNFCRF();
    }

    root = SD.open("/");

    gi_PN532.InitHardwareSPI(SPI_CS_PIN, RESET_PIN);

    InitReader(false);
}

void loop()
{
    	switch (gSMCurrentState) {
			case CARD_READ:
				SM_CardReading();
				break;

			case UPLOAD_DATA:
		    	/* state machine wait for TCP clients */
				OLEDScreen::ShowWiFi();
				WLAN::Initialize();
				FlashLED(LED_BUILTIN, 1000);
				if(true == WLAN::StartTCP())
				{
					OLEDScreen::ShowDT();
					if(true == WLAN::StartTransffer())
					{
						gSMCurrentState = BACKUP_DATA;
					}
					else
					{
						gSMCurrentState = CARD_READ;
						WLAN::ZeroInit();
					}
				}
				else
				{
					gSMCurrentState = CARD_READ;
					WLAN::ZeroInit();
					OLEDScreen::ShowReady();
					OLEDScreen::ShowNFCRF();
				}
				break;

			case BACKUP_DATA:
				WLAN::ZeroInit();
				OLEDScreen::ShowBackup();
#ifdef SKIP_BACKUP
				if(true == Utils::Backup_Data())
				{
#endif
					gSMCurrentState = CARD_READ;
#ifdef SKIP_BACKUP
				}
				else
				{
					gSMCurrentState = SDCARD_ERROR;
				}
#endif
				break;

			case SDCARD_ERROR:
				/* wait for RESET */
		    	OLEDScreen::ShowSDError();
		    	FlashLED(LED_BUILTIN, 5000);
				break;

			default:
				break;
		}
}


// CARD_READ handling function
void SM_CardReading(void)
{
	if (!gb_InitSuccess)
	{
		InitReader(true); // flash red LED for 2.4 seconds
		//Utils::Print("Init .");
	}
	else if (!ReadCard(k_User.ID.u8, &k_Card))
	{
		if (k_Card.b_PN532_Error) // Another error from PN532 -> reset the chip
		{
			//Utils::Print(" some error ");
			InitReader(true); // flash red LED for 2.4 seconds
		}
	}
	else if (k_Card.u8_UidLength == 0)
	{
		// No card present in the RF field
		gu64_LastID = 0;

		// Flash the green LED shortly. On Power Failure flash the red LED shortly.
		FlashLED(LED_BUILTIN, 100);
		OLEDScreen::ShowNFCRF();
	}
	else if (gu64_LastID == k_User.ID.u64)
	{
		// Still the same card present - Nothing to do
	}
	else if(	(0x000000BB36AB22ULL == k_User.ID.u64)
			||	(0x000000651B121CULL == k_User.ID.u64))
	{
		/*0x000000BB36AB22ULL - MASTER key found*/
		/*0x000000651B121CULL - ALT_MASTER key found*/
		gSMCurrentState = UPLOAD_DATA;
	}
	else
	{
#ifdef STD_PRINT_EN
		Utils::PrintHexBuf(&k_User.ID.u8[0], 7, LF);
#endif
		// A different card was found in the RF field
		if(Utils::UpdateSDCardCounter(k_User.ID.u64, &k_Card, 0))
		{
			// Avoid that the door is opened twice when the card is in the RF field for a longer time.
			gu64_LastID = k_User.ID.u64;
			FlashLED(LED_BUILTIN, 200);
		}
		else
		{
			// Error writing to SD Card - switch to SD Card error
			gSMCurrentState = SDCARD_ERROR;
		}
	}
}

// Reads the card in the RF field.
// In case of a Random ID card reads the real UID of the card (requires PICC authentication)
// ATTENTION: If no card is present, this function returns true. This is not an error. (check that pk_Card->u8_UidLength > 0)
// pk_Card->u8_KeyVersion is > 0 if a random ID card did a valid authentication with SECRET_PICC_MASTER_KEY
// pk_Card->b_PN532_Error is set true if the error comes from the PN532.
bool ReadCard(byte u8_UID[8], kCard* pk_Card)
{
    memset(pk_Card, 0, sizeof(kCard));

    if (!gi_PN532.ReadPassiveTargetID(u8_UID, &pk_Card->u8_UidLength, &pk_Card->e_CardType))
    {
        pk_Card->b_PN532_Error = true;
        return false;
    }
    return true;
}

// Use this for delays > 100 ms to guarantee that the battery is checked frequently
void LongDelay(int s32_Interval)
{
	#define min(a,b) ((a)<(b)?(a):(b))

    while(s32_Interval > 0)
    {
        int s32_Delay = s32_Interval;
        s32_Delay = min(100, s32_Interval);
        Utils::DelayMilli(s32_Delay);
        s32_Interval -= s32_Delay;
    }
}
