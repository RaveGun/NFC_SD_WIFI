
#ifndef UTILS_H
#define UTILS_H


#include <Arduino.h>
#include <SD.h>
#include <SSD1306.h>
#include <SSD1306Wire.h>

#define USE_HARDWARE_SPI   true
#if USE_HARDWARE_SPI
    #include <SPI.h>  // Hardware SPI bus
#endif

#define LF  "\r\n" // LineFeed 

// I2C pin configuration
#define OLED_SCL    D1
#define OLED_SDA    D2
#define OLED_ADDR   (0x3cu)
extern SSD1306 display;
extern File root;


// -------------------------------------------------------------------------------------------------------------------
// USB connection to Terminal program (Teraterm) on PC via COM port
// When you compile the code for Linux, Windows or any other platform you must modify this class.
// You can leave all functions empty and only redirect Print() to printf().
class SerialClass
{  
public:
    // Create a COM connection via USB.
    static inline void Begin(uint32_t u32_Baud) 
    {
        Serial.begin(u32_Baud);
    }
    // returns how many characters the user has typed in the Terminal program on the PC which have not yet been read with Read()
    static inline int Available()
    {
        return Serial.available();
    }
    // Get the next character from the Terminal program on the PC
	// returns -1 if no character available
    static inline int Read()
    {
        return Serial.read();
    }
    // Print text to the Terminal program on the PC
	// On Windows/Linux use printf() here to write debug output an errors to the Console.
    static inline void Print(const char* s8_Text)
    {
        Serial.print(s8_Text);
    }
};

// -------------------------------------------------------------------------------------------------------------------

class OLEDScreen
{
public:
	static void Initialize(void);
	static void ShowReady(void);
	static void ShowSDError(void);
	static void ShowWiFi(void);
	static void ShowDT(void);
	static void ShowBackup(void);
	static void ShowProgressBar(uint16_t currentVal, uint16_t totalVal);
};

// -------------------------------------------------------------------------------------------------------------------

class WLAN
{
public:
	static void ZeroInit(void);
	static void Initialize(void);
	static bool StartTCP(void);
	static bool StartTransffer(void);
	static bool ReadSecretKey(char RcvDate[9]);
};
// -------------------------------------------------------------------------------------------------------------------

// This class implements Hardware SPI (4 wire bus). It is not used for the DoorOpener sketch.
// When you compile the code for Linux, Windows or any other platform you must modify this class.
// NOTE: This class is not used when you switched to I2C mode with PN532::InitI2C() or Software SPI mode with PN532::InitSoftwareSPI().
class SpiClass
{
public:
	static inline void Begin(uint32_t u32_Clock)
	{
		SPI.begin();
		SPI.beginTransaction(SPISettings(u32_Clock, LSBFIRST, SPI_MODE0));
	}
	// Write one byte to the MOSI pin and at the same time receive one byte on the MISO pin.
	static inline byte Transfer(byte u8_Data)
	{
		return SPI.transfer(u8_Data);
	}
};

// -------------------------------------------------------------------------------------------------------------------

class Utils
{
public:
    // returns the current tick counter
	// When you compile the code for Linux, Windows or any other platform you must change this function.
	// On Windows use GetTickCount() here
    static inline uint32_t GetMillis()
    {
        return millis();
    }

	// When you compile the code for Linux, Windows or any other platform you must change this function.
	// Use Sleep() here.
    static inline void DelayMilli(int s32_MilliSeconds)
    {
        delay(s32_MilliSeconds);
    }

	// This function is only required for Software SPI mode.
    // When you compile the code for Linux, Windows or any other platform you must change this function.
	// There is no API in Windows that supports delays shorter than approx 20 milli seconds. (Sleep(1) will sleep approx 20 ms)
	// To implement delays in micro seconds you can use a loop that runs until a performance counter has reached the expected value.
	// On Windows use: while(...) { .. QueryPerformanceCounter() .. if (Counter > X) break; .. }
    static inline void DelayMicro(int s32_MicroSeconds)
    {
        delayMicroseconds(s32_MicroSeconds);
    }
    
	// Defines if a digital processor pin is used as input or output
    // u8_Mode = INPUT or OUTPUT
    // When you compile the code for Linux, Windows or any other platform you must change this function.	
    static inline void SetPinMode(byte u8_Pin, byte u8_Mode)
    {
        pinMode(u8_Pin, u8_Mode);
    }
    
	// Sets a digital processor pin high or low.
    // u8_Status = HIGH or LOW
	// When you compile the code for Linux, Windows or any other platform you must change this function.
    static inline void WritePin(byte u8_Pin, byte u8_Status)
    {
        digitalWrite(u8_Pin, u8_Status);
    }

	// reads the current state of a digital processor pin.
    // returns HIGH or LOW
    // When you compile the code for Linux, Windows or any other platform you must change this function.	
    static inline byte ReadPin(byte u8_Pin)
    {
        return digitalRead(u8_Pin);
    }

    static uint64_t GetMillis64();
    static void     Base36(uint64_t u64_ID, char *s8_LF);
    static void     Print(const char*   s8_Text,  const char* s8_LF=NULL);
    static void     PrintDec  (int      s32_Data, const char* s8_LF=NULL);
    static void     PrintHex8 (byte     u8_Data,  const char* s8_LF=NULL);
    static void     PrintHex16(uint16_t u16_Data, const char* s8_LF=NULL);
    static void     PrintHex32(uint32_t u32_Data, const char* s8_LF=NULL);
    static void     PrintHexBuf(const byte* u8_Data, const uint32_t u32_DataLen, const char* s8_LF=NULL, int s32_Brace1=-1, int S32_Brace2=-1);
    static void     GetHexBuf(const byte* u8_Data, const uint32_t u32_DataLen, String* retVal);
    static void     PrintInterval(uint64_t u64_Time, const char* s8_LF=NULL);
    static void     GenerateRandom(byte* u8_Random, int s32_Length);
    static void     RotateBlockLeft(byte* u8_Out, const byte* u8_In, int s32_Length);
    static void     BitShiftLeft(uint8_t* u8_Data, int s32_Length);
    static void     XorDataBlock(byte* u8_Out,  const byte* u8_In, const byte* u8_Xor, int s32_Length);    
    static void     XorDataBlock(byte* u8_Data, const byte* u8_Xor, int s32_Length);
    static uint16_t CalcCrc16(const byte* u8_Data,  int s32_Length);
    static uint32_t CalcCrc32(const byte* u8_Data1, int s32_Length1, const byte* u8_Data2=NULL, int s32_Length2=0);
    static void     GetSDCounterForCard(char* fileName, uint16_t * u16_noOfCoffees);
    static bool     UpdateSDCardCounter(uint64_t u64_ID, kCard* pk_Card, uint64_t u64_StartTick);
    static uint16_t getNumFiles(File dir);
    static bool     printDirectory(File dir, uint16_t fileCount);
	static bool		Backup_Data(void);
private:
    static uint32_t CalcCrc32(const byte* u8_Data, int s32_Length, uint32_t u32_Crc);
};

#endif // UTILS_H
