#include <stdlib.h>
#include <string.h>
#include "KernalSDCard.h"


#define SD_REGISTERS			0x400
#define SD_ADDRESS_LOW			0x00
#define SD_ADDRESS_HIGH			0x01
#define SD_ADDRESS_BANK			0x02
#define SD_GPIO					0x04
#define SD_DATA_COUNT_LOW		0x06
#define SD_DATA_COUNT_HIGH		0x07
#define SD_CRC_COUNT_LOW		0x08
#define SD_CRC_COUNT_HIGH		0x09
#define SD_CLEAR_INTERRUPT		0x0A
#define SD_READ_START			0x0B
#define SD_FLAGS				0x0C
#define SD_WRITE_DATA			0x0E

#define SD_CLK					0x01
#define SD_CMD					0x02
#define SD_DAT0					0x04
#define SD_DRIVE_CLK			0x10
#define SD_DRIVE_CMD			0x20

#define SD_FLAG_CARD			0x03		//The socket.
#define SD_FLAG_ENABLE			0x40
#define SD_FLAG_INTERRUPT		0x80

#define SD_SECTOR_SIZE			512
#define SD_MAIN_MEMORY			0x200000	//Kernel mode addresses from here on are main memory, where the board's DMA reaches.
#define SD_CRC_BYTES			8			//The four lines' CRC16s, two nybbles a byte.
#define SD_RESPONSE_WAIT		64			//Clocks to wait for a response's start bit (Ncr).
#define SD_READ_WAIT			60000		//Polls of the flags for a block read.
#define SD_ACMD41_TRIES			1000

#define SD_R1_ERRORS			0xFDF90008	//Every R1 error bit.


//The board's GPIO register, for KernalSDCard.asm's bit banging.
extern "C" volatile uint8*	gpuiKernalSDGPIO;
volatile uint8*				gpuiKernalSDGPIO;

extern "C" void		KernalSDSendBits(const uint8* puiBytes, uint16 uiBytes);
extern "C" void		KernalSDReceiveBits(uint8* puiBytes, uint16 uiBits);
extern "C" uint16	KernalSDWaitStart(uint16 uiClocks);
extern "C" void		KernalSDClocks(uint16 uiClocks);


//x^7 + x^3 + 1, a byte at a time: the CRC is kept shifted up a bit (in D7-D1, D0 zero), and the next is
//gauiKernalCRC7[CRC ^ byte].  Each entry is the CRC of its index, shifted up.
const uint8 gauiKernalCRC7[256] =
{
	0x00, 0x12, 0x24, 0x36, 0x48, 0x5A, 0x6C, 0x7E, 0x90, 0x82, 0xB4, 0xA6, 0xD8, 0xCA, 0xFC, 0xEE,
	0x32, 0x20, 0x16, 0x04, 0x7A, 0x68, 0x5E, 0x4C, 0xA2, 0xB0, 0x86, 0x94, 0xEA, 0xF8, 0xCE, 0xDC,
	0x64, 0x76, 0x40, 0x52, 0x2C, 0x3E, 0x08, 0x1A, 0xF4, 0xE6, 0xD0, 0xC2, 0xBC, 0xAE, 0x98, 0x8A,
	0x56, 0x44, 0x72, 0x60, 0x1E, 0x0C, 0x3A, 0x28, 0xC6, 0xD4, 0xE2, 0xF0, 0x8E, 0x9C, 0xAA, 0xB8,
	0xC8, 0xDA, 0xEC, 0xFE, 0x80, 0x92, 0xA4, 0xB6, 0x58, 0x4A, 0x7C, 0x6E, 0x10, 0x02, 0x34, 0x26,
	0xFA, 0xE8, 0xDE, 0xCC, 0xB2, 0xA0, 0x96, 0x84, 0x6A, 0x78, 0x4E, 0x5C, 0x22, 0x30, 0x06, 0x14,
	0xAC, 0xBE, 0x88, 0x9A, 0xE4, 0xF6, 0xC0, 0xD2, 0x3C, 0x2E, 0x18, 0x0A, 0x74, 0x66, 0x50, 0x42,
	0x9E, 0x8C, 0xBA, 0xA8, 0xD6, 0xC4, 0xF2, 0xE0, 0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54, 0x62, 0x70,
	0x82, 0x90, 0xA6, 0xB4, 0xCA, 0xD8, 0xEE, 0xFC, 0x12, 0x00, 0x36, 0x24, 0x5A, 0x48, 0x7E, 0x6C,
	0xB0, 0xA2, 0x94, 0x86, 0xF8, 0xEA, 0xDC, 0xCE, 0x20, 0x32, 0x04, 0x16, 0x68, 0x7A, 0x4C, 0x5E,
	0xE6, 0xF4, 0xC2, 0xD0, 0xAE, 0xBC, 0x8A, 0x98, 0x76, 0x64, 0x52, 0x40, 0x3E, 0x2C, 0x1A, 0x08,
	0xD4, 0xC6, 0xF0, 0xE2, 0x9C, 0x8E, 0xB8, 0xAA, 0x44, 0x56, 0x60, 0x72, 0x0C, 0x1E, 0x28, 0x3A,
	0x4A, 0x58, 0x6E, 0x7C, 0x02, 0x10, 0x26, 0x34, 0xDA, 0xC8, 0xFE, 0xEC, 0x92, 0x80, 0xB6, 0xA4,
	0x78, 0x6A, 0x5C, 0x4E, 0x30, 0x22, 0x14, 0x06, 0xE8, 0xFA, 0xCC, 0xDE, 0xA0, 0xB2, 0x84, 0x96,
	0x2E, 0x3C, 0x0A, 0x18, 0x66, 0x74, 0x42, 0x50, 0xBE, 0xAC, 0x9A, 0x88, 0xF6, 0xE4, 0xD2, 0xC0,
	0x1C, 0x0E, 0x38, 0x2A, 0x54, 0x46, 0x70, 0x62, 0x8C, 0x9E, 0xA8, 0xBA, 0xC4, 0xD6, 0xE0, 0xF2,
};


//////////////////////////////////////////////////////////////////////////
//
// x^7 + x^3 + 1.
//
//////////////////////////////////////////////////////////////////////////
uint8 KernalCRC7(uint8* puiData, uint16 uiLength)
{
	uint8	uiCRC;
	uint16	i;

	uiCRC = 0;
	for (i = 0; i < uiLength; i++)
	{
		uiCRC = gauiKernalCRC7[uiCRC ^ puiData[i]];
	}
	return (uint8)(uiCRC >> 1);
}


//////////////////////////////////////////////////////////////////////////
//
// Brings the card in socket 0 up: 80 clocks, CMD0, CMD8, ACMD41 until
// ready, CMD2, CMD3, CMD9 (its size), CMD7 and ACMD6 (4 bit bus).  Returns
// KERNAL_SD_SUCCESS or why it failed.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalSDCard::Init(SKernalDevice* psDevice, uint8 uiSocket)
{
	uint16	i;
	uint32	uiSize;

	CFileDrive::Init(SD_SECTOR_SIZE);
	mpuiRegisters = (volatile uint8*)(psDevice->uiAddress + SD_REGISTERS);
	gpuiKernalSDGPIO = &mpuiRegisters[SD_GPIO];
	mpuiBounce = NULL;
	muiRCA = 0;
	muiMaxSector = 0;
	muiError = KERNAL_SD_SUCCESS;
	mbIdentification = true;

	//GPIO first: its latch isn't reset, and enabling the board turns its drivers on.
	SetGPIO(SD_CLK | SD_CMD | SD_DRIVE_CLK | SD_DRIVE_CMD);
	muiFlags = (uint8)(SD_FLAG_ENABLE | (uiSocket & SD_FLAG_CARD));
	Select();
	Clocks(80);

	SendCommand(0, 0);
	Clocks(8);

	if (!Command(8, 0x1AA, 48) || ((ResponseContent() & 0xFFF) != 0x1AA))
	{
		muiError = KERNAL_SD_NO_CARD;
		return muiError;
	}

	for (i = 0; i < SD_ACMD41_TRIES; i++)
	{
		if (!CommandR1(55, 0) || !Command(41, 0x40FF8000, 48))
		{
			return muiError;
		}
		if (ResponseContent() & 0x80000000)
		{
			break;
		}
	}
	if (i == SD_ACMD41_TRIES)
	{
		muiError = KERNAL_SD_NOT_READY;
		return muiError;
	}

	if (!Command(2, 0, 136) || !Command(3, 0, 48))
	{
		return muiError;
	}
	muiRCA = (uint16)(ResponseContent() >> 16);
	mbIdentification = false;

	//CSD version 2: C_SIZE (bits 69 to 48) + 1 times 1024 sectors.
	if (!Command(9, (uint32)muiRCA << 16, 136))
	{
		return muiError;
	}
	uiSize = ((uint32)(mauiResponse[8] & 0x3F) << 16) | ((uint32)mauiResponse[9] << 8) | mauiResponse[10];
	muiMaxSector = ((uint64)uiSize + 1) * 1024;

	if (!CommandR1(7, (uint32)muiRCA << 16) || !CommandR1(55, (uint32)muiRCA << 16) || !CommandR1(6, 2))
	{
		return muiError;
	}

	//Only for sectors read to (or written from) memory the board can't reach.
	mpuiBounce = (uint8*)malloc(SD_SECTOR_SIZE);
	return KERNAL_SD_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// The board's bus switch connects this card's socket (the board is shared
// by the cards in its four sockets).
//
//////////////////////////////////////////////////////////////////////////
void CKernalSDCard::Select(void)
{
	mpuiRegisters[SD_FLAGS] = muiFlags;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalSDCard::Kill(void)
{
	mpuiRegisters[SD_FLAGS] = 0;
	if (mpuiBounce)
	{
		free(mpuiBounce);
		mpuiBounce = NULL;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalSDCard::SetGPIO(uint8 uiValue)
{
	mpuiRegisters[SD_GPIO] = uiValue;
}


//////////////////////////////////////////////////////////////////////////
//
// Clocks with CMD driven high.
//
//////////////////////////////////////////////////////////////////////////
void CKernalSDCard::Clocks(uint16 uiClocks)
{
	uint16	i;

	if (!mbIdentification)
	{
		KernalSDClocks(uiClocks);
		return;
	}
	for (i = 0; i < uiClocks; i++)
	{
		SetGPIO(SD_CMD | SD_DRIVE_CLK | SD_DRIVE_CMD);
		SetGPIO(SD_CLK | SD_CMD | SD_DRIVE_CLK | SD_DRIVE_CMD);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The 48 bits, each put on CMD while CLK is low and taken by the card as
// CLK rises.  CLK is left high.
//
//////////////////////////////////////////////////////////////////////////
void CKernalSDCard::SendCommand(uint8 uiIndex, uint32 uiArgument)
{
	uint8	auiCommand[6];
	uint8	uiByte;
	uint8	uiLevel;
	uint16	i;
	uint8	uiBit;

	auiCommand[0] = (uint8)(0x40 | uiIndex);
	auiCommand[1] = (uint8)(uiArgument >> 24);
	auiCommand[2] = (uint8)(uiArgument >> 16);
	auiCommand[3] = (uint8)(uiArgument >> 8);
	auiCommand[4] = (uint8)uiArgument;
	auiCommand[5] = (uint8)(gauiKernalCRC7[gauiKernalCRC7[gauiKernalCRC7[gauiKernalCRC7[gauiKernalCRC7[auiCommand[0]] ^ auiCommand[1]] ^ auiCommand[2]] ^ auiCommand[3]] ^ auiCommand[4]] | 1);
	if (!mbIdentification)
	{
		KernalSDSendBits(auiCommand, 6);
		return;
	}
	for (i = 0; i < 6; i++)
	{
		uiByte = auiCommand[i];
		for (uiBit = 0; uiBit < 8; uiBit++)
		{
			uiLevel = (uiByte & 0x80) ? SD_CMD : 0;
			SetGPIO((uint8)(uiLevel | SD_DRIVE_CLK | SD_DRIVE_CMD));
			SetGPIO((uint8)(SD_CLK | uiLevel | SD_DRIVE_CLK | SD_DRIVE_CMD));
			uiByte = (uint8)(uiByte << 1);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// CMD is let go and the card clocked until it pulls CMD low (the start
// bit); then the rest of uiBits bits are read into mauiResponse.  The card
// changes CMD as CLK falls; it is read once CLK has risen.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalSDCard::ReadResponse(uint16 uiBits)
{
	uint16	i;
	uint8	uiByte;

	if (!mbIdentification)
	{
		if (KernalSDWaitStart(SD_RESPONSE_WAIT) == 0)
		{
			muiError = KERNAL_SD_NO_RESPONSE;
			return false;
		}

		//The start bit is the first byte's top bit; uiBits is a multiple of 8.
		KernalSDReceiveBits(mauiResponse, (uint16)(uiBits - 1));
		Clocks(8);
		return true;
	}

	memset(mauiResponse, 0, sizeof(mauiResponse));
	for (i = 0; i < SD_RESPONSE_WAIT; i++)
	{
		SetGPIO(SD_DRIVE_CLK);
		SetGPIO(SD_CLK | SD_DRIVE_CLK);
		if ((mpuiRegisters[SD_GPIO] & SD_CMD) == 0)
		{
			break;
		}
	}
	if (i == SD_RESPONSE_WAIT)
	{
		muiError = KERNAL_SD_NO_RESPONSE;
		return false;
	}

	uiByte = 0;
	for (i = 1; i < uiBits; i++)
	{
		SetGPIO(SD_DRIVE_CLK);
		SetGPIO(SD_CLK | SD_DRIVE_CLK);
		uiByte = (uint8)(uiByte << 1);
		if (mpuiRegisters[SD_GPIO] & SD_CMD)
		{
			uiByte |= 1;
		}
		if ((i & 7) == 7)
		{
			mauiResponse[i >> 3] = uiByte;
			uiByte = 0;
		}
	}

	//A few clocks for the card to finish, with CMD driven high again.
	Clocks(8);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CKernalSDCard::Command(uint8 uiIndex, uint32 uiArgument, uint16 uiResponseBits)
{
	SendCommand(uiIndex, uiArgument);
	return ReadResponse(uiResponseBits);
}


//////////////////////////////////////////////////////////////////////////
//
// A command with an R1 response, which must have no error bits.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalSDCard::CommandR1(uint8 uiIndex, uint32 uiArgument)
{
	if (!Command(uiIndex, uiArgument, 48))
	{
		return false;
	}
	if (ResponseContent() & SD_R1_ERRORS)
	{
		muiError = KERNAL_SD_COMMAND_ERROR;
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The 32 bits after a 48 bit response's index.
//
//////////////////////////////////////////////////////////////////////////
uint32 CKernalSDCard::ResponseContent(void)
{
	return ((uint32)mauiResponse[1] << 24) | ((uint32)mauiResponse[2] << 16) | ((uint32)mauiResponse[3] << 8) | mauiResponse[4];
}


//////////////////////////////////////////////////////////////////////////
//
// CMD17 (one sector) or CMD18 (several, stopped with CMD12), then for each
// sector CLK let go and Read Start: the board clocks the block into main
// memory and raises its interrupt flag.  The board stops SD CLK after each
// block, so the card waits for the next Read Start.  Sectors go straight
// to puiData when it is main memory, else through the bounce buffer.  The
// board's address counts on through the blocks.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalSDCard::ReadSectors(uint32 uiSector, uint32 uiNumSectors, uint8* puiData)
{
	uint32	uiAddress;
	uint32	uiBlock;
	uint16	i;
	bool	bDirect;
	bool	bResult;

	if (uiNumSectors == 0)
	{
		return true;
	}
	bDirect = ((uint32)puiData >= SD_MAIN_MEMORY);
	if (!bDirect && !mpuiBounce)
	{
		muiError = KERNAL_SD_NO_CARD;
		return false;
	}

	uiAddress = bDirect ? (uint32)puiData : (uint32)mpuiBounce;
	mpuiRegisters[SD_ADDRESS_LOW] = (uint8)uiAddress;
	mpuiRegisters[SD_ADDRESS_HIGH] = (uint8)(uiAddress >> 8);
	mpuiRegisters[SD_ADDRESS_BANK] = (uint8)(uiAddress >> 16);
	mpuiRegisters[SD_DATA_COUNT_LOW] = (uint8)SD_SECTOR_SIZE;
	mpuiRegisters[SD_DATA_COUNT_HIGH] = (uint8)(SD_SECTOR_SIZE >> 8);
	mpuiRegisters[SD_CRC_COUNT_LOW] = SD_CRC_BYTES;
	mpuiRegisters[SD_CRC_COUNT_HIGH] = 0;
	mpuiRegisters[SD_FLAGS] = (uint8)(muiFlags | SD_FLAG_INTERRUPT);

	if (!CommandR1((uiNumSectors == 1) ? 17 : 18, uiSector))
	{
		mpuiRegisters[SD_FLAGS] = muiFlags;
		return false;
	}

	bResult = true;
	for (uiBlock = 0; uiBlock < uiNumSectors; uiBlock++)
	{
		if (!bDirect && (uiBlock > 0))
		{
			uiAddress = (uint32)mpuiBounce;
			mpuiRegisters[SD_ADDRESS_LOW] = (uint8)uiAddress;
			mpuiRegisters[SD_ADDRESS_HIGH] = (uint8)(uiAddress >> 8);
			mpuiRegisters[SD_ADDRESS_BANK] = (uint8)(uiAddress >> 16);
		}
		SetGPIO(SD_CMD | SD_DRIVE_CMD);
		mpuiRegisters[SD_CLEAR_INTERRUPT] = 0;
		mpuiRegisters[SD_READ_START] = 0;
		for (i = 0; i < SD_READ_WAIT; i++)
		{
			if (mpuiRegisters[SD_FLAGS] & SD_FLAG_INTERRUPT)
			{
				break;
			}
		}
		if (i == SD_READ_WAIT)
		{
			muiError = KERNAL_SD_READ_TIMEOUT;
			bResult = false;
			break;
		}
		if (!bDirect)
		{
			memcpy(&puiData[uiBlock * SD_SECTOR_SIZE], mpuiBounce, SD_SECTOR_SIZE);
		}
	}
	mpuiRegisters[SD_FLAGS] = muiFlags;
	SetGPIO(SD_CLK | SD_CMD | SD_DRIVE_CLK | SD_DRIVE_CMD);

	if (uiNumSectors > 1)
	{
		//CMD12 stops the card sending blocks.
		bResult = CommandR1(12, 0) && bResult;
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// CMD24, then the DAT stream through Write Data, then the card's CRC
// status on DAT0 (010 is accepted) and its busy (DAT0 low) clocked out.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalSDCard::WriteSector(uint32 uiSector, void* pvData)
{
	uint8*	puiData;
	uint16	auiCRC[4];
	uint16	i;
	uint8	uiNybble;
	uint8	uiLine;
	uint16	uiIn;
	uint8	uiPrevious;
	uint8	auiTail[18];
	uint8	uiStatus;
	uint8	uiBit;
	volatile uint8*	puiWrite;

	if (!CommandR1(24, uiSector))
	{
		return false;
	}
	SetGPIO(SD_CMD | SD_DRIVE_CMD);

	//The four lines' CRC16s, over the nybbles high first.
	puiData = (uint8*)pvData;
	auiCRC[0] = auiCRC[1] = auiCRC[2] = auiCRC[3] = 0;
	for (i = 0; i < SD_SECTOR_SIZE * 2; i++)
	{
		uiNybble = (i & 1) ? (uint8)(puiData[i >> 1] & 0xF) : (uint8)(puiData[i >> 1] >> 4);
		for (uiLine = 0; uiLine < 4; uiLine++)
		{
			uiIn = (uint16)(((uiNybble >> uiLine) ^ (auiCRC[uiLine] >> 15)) & 1);
			auiCRC[uiLine] = (uint16)(auiCRC[uiLine] << 1);
			if (uiIn)
			{
				auiCRC[uiLine] ^= 0x1021;
			}
		}
	}

	//The stream is one nybble behind the data: the start nybble (0) first, the end nybble (F) last.
	puiWrite = &mpuiRegisters[SD_WRITE_DATA];
	uiPrevious = 0;
	for (i = 0; i < SD_SECTOR_SIZE; i++)
	{
		*puiWrite = (uint8)((uiPrevious << 4) | (puiData[i] >> 4));
		uiPrevious = (uint8)(puiData[i] & 0xF);
	}
	//What's left: the last data nybble, the 16 CRC nybbles and the end nybble, 9 bytes.
	auiTail[0] = uiPrevious;
	for (i = 0; i < 16; i++)
	{
		uiNybble = 0;
		for (uiLine = 0; uiLine < 4; uiLine++)
		{
			if (auiCRC[uiLine] & (0x8000 >> i))
			{
				uiNybble |= (uint8)(1 << uiLine);
			}
		}
		auiTail[1 + i] = uiNybble;
	}
	auiTail[17] = 0xF;
	for (i = 0; i < 18; i += 2)
	{
		*puiWrite = (uint8)((auiTail[i] << 4) | auiTail[i + 1]);
	}

	//CRC status: a start bit (0), three status bits and an end bit; then busy until DAT0 is high.
	for (i = 0; i < 16; i++)
	{
		SetGPIO(SD_CMD | SD_DRIVE_CLK | SD_DRIVE_CMD);
		SetGPIO(SD_CLK | SD_CMD | SD_DRIVE_CLK | SD_DRIVE_CMD);
		if ((mpuiRegisters[SD_GPIO] & SD_DAT0) == 0)
		{
			break;
		}
	}
	uiStatus = 0;
	for (uiBit = 0; uiBit < 4; uiBit++)
	{
		SetGPIO(SD_CMD | SD_DRIVE_CLK | SD_DRIVE_CMD);
		SetGPIO(SD_CLK | SD_CMD | SD_DRIVE_CLK | SD_DRIVE_CMD);
		uiStatus = (uint8)((uiStatus << 1) | ((mpuiRegisters[SD_GPIO] & SD_DAT0) ? 1 : 0));
	}
	for (i = 0; i < 1000; i++)
	{
		SetGPIO(SD_CMD | SD_DRIVE_CLK | SD_DRIVE_CMD);
		SetGPIO(SD_CLK | SD_CMD | SD_DRIVE_CLK | SD_DRIVE_CMD);
		if (mpuiRegisters[SD_GPIO] & SD_DAT0)
		{
			break;
		}
	}
	if ((uiStatus >> 1) != 0x2)
	{
		muiError = KERNAL_SD_WRITE_REJECTED;
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CKernalSDCard::Read(uint64 uiSector, void* pvData)
{
	if (uiSector >= muiMaxSector)
	{
		muiError = KERNAL_SD_OUT_OF_RANGE;
		return false;
	}
	Select();
	return ReadSectors((uint32)uiSector, 1, (uint8*)pvData);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CKernalSDCard::Read(uint64 uiSector, uint32 uiNumSectors, void* pvData)
{
	if ((uiSector >= muiMaxSector) || (uiNumSectors > muiMaxSector - uiSector))
	{
		muiError = KERNAL_SD_OUT_OF_RANGE;
		return false;
	}
	Select();
	return ReadSectors((uint32)uiSector, uiNumSectors, (uint8*)pvData);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CKernalSDCard::Write(uint64 uiSector, void* pvData)
{
	if (uiSector >= muiMaxSector)
	{
		muiError = KERNAL_SD_OUT_OF_RANGE;
		return false;
	}
	Select();
	return WriteSector((uint32)uiSector, pvData);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CKernalSDCard::Write(uint64 uiSector, uint32 uiNumSectors, void* pvData)
{
	uint32	i;

	for (i = 0; i < uiNumSectors; i++)
	{
		if (!Write(uiSector + i, (uint8*)pvData + (lsize)i * SD_SECTOR_SIZE))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// SD cards don't need erasing before a write.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalSDCard::Erase(uint64 uiStartSector, uint64 uiStopSectorInclusive)
{
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalSDCard::GetError(void)
{
	return muiError;
}
