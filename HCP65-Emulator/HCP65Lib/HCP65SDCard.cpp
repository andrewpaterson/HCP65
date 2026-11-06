#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/PointerFunctions.h"
#include "HCP65.h"
#include "HCP65SDCard.h"


#define HCP65_SD_SECTOR_SIZE		512
#define HCP65_SD_WRITE_NYBBLES		1042		//Start, 1024 data, 16 CRC and end nybbles.
#define HCP65_SD_ACCESS_CLOCKS		8			//SD clocks from Read Start to the data's start bit (Nac).
#define HCP65_SD_RESPONSE_DELAY		2			//SD clocks between a command's end bit and its response (Ncr).
#define HCP65_SD_RCA				0x1234

//Card status bits (R1).
#define HCP65_SD_OUT_OF_RANGE		0x80000000
#define HCP65_SD_COM_CRC_ERROR		0x00800000
#define HCP65_SD_ILLEGAL_COMMAND	0x00400000
#define HCP65_SD_READY_FOR_DATA		0x00000100
#define HCP65_SD_APP_CMD			0x00000020
#define HCP65_SD_CLEARED_ON_READ	(HCP65_SD_OUT_OF_RANGE | HCP65_SD_COM_CRC_ERROR | HCP65_SD_ILLEGAL_COMMAND)


static const uint8 gauiHCP65SDCID[15] = { 0x03, 'H', 'C', 'H', 'C', 'P', '6', '5', 0x10, 0x00, 0x00, 0x00, 0x01, 0x01, 0xA4 };


//////////////////////////////////////////////////////////////////////////
//
// The sockets start empty.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::Init(const char* szIdentifier)
{
	size	uiLength;
	int		i;

	CHCP65Device::Init(HCP65_SMALL_SLOT_SIZE);
	memset(mauiROM, 0xFF, HCP65_SD_ROM_SIZE);
	uiLength = strlen(szIdentifier);
	if (uiLength >= HCP65_SD_ROM_SIZE)
	{
		uiLength = HCP65_SD_ROM_SIZE - 1;
	}
	memcpy(mauiROM, szIdentifier, uiLength);
	mauiROM[uiLength] = 0;

	for (i = 0; i < HCP65_SD_NUM_SOCKETS; i++)
	{
		masSockets[i].puiImage = NULL;
		masSockets[i].uiNumSectors = 0;
		masSockets[i].bWriteBack = false;
		masSockets[i].szFilename.Init();
	}
	mpsCard = &masSockets[0];
	mpsReading = NULL;
	muiGPIO = 0;
	muiAddress = 0;
	muiDataCount = 0;
	muiCRCCount = 0;
	muiSectorsRead = 0;
	muiSectorsWritten = 0;
	Reset();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::Kill(void)
{
	int		i;

	for (i = 0; i < HCP65_SD_NUM_SOCKETS; i++)
	{
		RemoveCard(i);
		masSockets[i].szFilename.Kill();
	}
	CHCP65Device::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The board is disabled with socket 0 selected; the cards go back to idle
// (a reset of the system doesn't power cycle them, but a card is only used
// after CMD0).  The GPIO latch and the address and count latches are not
// reset.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::Reset(void)
{
	int		i;

	muiFlags = 0;
	mbEnabled = false;
	mbInterrupt = false;
	mbReading = false;
	mpsReading = NULL;
	muiReadDone = 0;
	muiNumWriteNybbles = 0;
	mbPreviousCLK = true;
	mpsCard = &masSockets[0];
	for (i = 0; i < HCP65_SD_NUM_SOCKETS; i++)
	{
		ResetCard(&masSockets[i]);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::ResetCard(SHCP65SDCardSocket* psCard)
{
	psCard->eState = HSDCS_Idle;
	psCard->bApplicationCommand = false;
	psCard->uiRCA = 0;
	psCard->uiStatus = 0;
	psCard->bWideBus = false;
	psCard->uiCommandBits = 0;
	psCard->uiResponseBits = 0;
	psCard->uiResponseSent = 0;
	psCard->uiResponseDelay = 0;
	psCard->uiReadSector = 0;
	psCard->bReadPending = false;
	psCard->bReadMultiple = false;
	psCard->uiWriteSector = 0;
	psCard->bWritePending = false;
	psCard->uiNumDAT0Bits = 0;
	psCard->uiDAT0Sent = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads a whole image file into memory as the card in iSocket (0 to 3);
// bWriteBack writes it back on Flush.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65SDCard::InsertCard(const char* szImageFilename, bool bWriteBack, int iSocket)
{
	CFileUtil			cFileUtil;
	size				uiSize;
	SHCP65SDCardSocket*	psCard;

	if ((iSocket < 0) || (iSocket >= HCP65_SD_NUM_SOCKETS))
	{
		return false;
	}
	RemoveCard(iSocket);
	psCard = &masSockets[iSocket];
	psCard->puiImage = (uint8*)cFileUtil.ReadAll(szImageFilename, &uiSize);
	if (!psCard->puiImage)
	{
		return false;
	}
	if ((uiSize == 0) || (uiSize % HCP65_SD_SECTOR_SIZE != 0))
	{
		SafeFree(psCard->puiImage);
		return false;
	}
	psCard->uiNumSectors = (uint32)(uiSize / HCP65_SD_SECTOR_SIZE);
	psCard->bWriteBack = bWriteBack;
	psCard->szFilename.Kill();
	psCard->szFilename.Init(szImageFilename);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A copy of uiSize bytes of puiImage as the card in iSocket.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65SDCard::InsertCard(uint8* puiImage, uint32 uiSize, int iSocket)
{
	SHCP65SDCardSocket*	psCard;

	if ((iSocket < 0) || (iSocket >= HCP65_SD_NUM_SOCKETS))
	{
		return false;
	}
	RemoveCard(iSocket);
	if ((uiSize == 0) || (uiSize % HCP65_SD_SECTOR_SIZE != 0))
	{
		return false;
	}
	psCard = &masSockets[iSocket];
	psCard->puiImage = (uint8*)malloc(uiSize);
	memcpy(psCard->puiImage, puiImage, uiSize);
	psCard->uiNumSectors = uiSize / HCP65_SD_SECTOR_SIZE;
	psCard->bWriteBack = false;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::RemoveCard(int iSocket)
{
	SHCP65SDCardSocket*	psCard;

	if ((iSocket < 0) || (iSocket >= HCP65_SD_NUM_SOCKETS))
	{
		return;
	}
	psCard = &masSockets[iSocket];
	if (mpsReading == psCard)
	{
		mbReading = false;
		mpsReading = NULL;
	}
	FlushCard(psCard);
	SafeFree(psCard->puiImage);
	psCard->uiNumSectors = 0;
	psCard->bWriteBack = false;
	ResetCard(psCard);
}


//////////////////////////////////////////////////////////////////////////
//
// Writes each image back to its file if it was inserted with bWriteBack.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65SDCard::Flush(void)
{
	bool	bResult;
	int		i;

	bResult = true;
	for (i = 0; i < HCP65_SD_NUM_SOCKETS; i++)
	{
		bResult &= FlushCard(&masSockets[i]);
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65SDCard::FlushCard(SHCP65SDCardSocket* psCard)
{
	FILE*	pcFile;
	size_t	uiSize;
	bool	bResult;

	if (!psCard->puiImage || !psCard->bWriteBack || psCard->szFilename.Empty())
	{
		return true;
	}
	pcFile = fopen(psCard->szFilename.Text(), "r+b");
	if (!pcFile)
	{
		return false;
	}
	uiSize = (size_t)psCard->uiNumSectors * HCP65_SD_SECTOR_SIZE;
	bResult = fwrite(psCard->puiImage, 1, uiSize, pcFile) == uiSize;
	fclose(pcFile);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the bus reaches a card: the board is enabled and the selected
// socket has one.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65SDCard::HasCard(void)
{
	return mbEnabled && (mpsCard->puiImage != NULL);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65SDCard::Read(uint32 uiOffset)
{
	if (uiOffset < HCP65_SD_ROM_SIZE)
	{
		return mauiROM[uiOffset];
	}
	if (uiOffset < 0x800)
	{
		return ReadRegister(uiOffset & 0x1F);
	}
	return 0xFF;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::Write(uint32 uiOffset, uint8 uiValue)
{
	if ((uiOffset >= HCP65_SD_ROM_SIZE) && (uiOffset < 0x800))
	{
		WriteRegister(uiOffset & 0x1F, uiValue);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The levels the card drives (CMD while responding, DAT0 after a write);
// a line nothing drives is pulled up.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65SDCard::CardDrivenLines(void)
{
	uint8	uiLines;
	uint8	uiByte;
	uint8	uiBit;

	uiLines = HCP65_SD_GPIO_CLK | HCP65_SD_GPIO_CMD | HCP65_SD_GPIO_DAT0 | HCP65_SD_GPIO_DAT3;
	if (!HasCard())
	{
		return uiLines;
	}
	if ((mpsCard->uiResponseBits != 0) && (mpsCard->uiResponseDelay == 0) && (mpsCard->uiResponseSent < mpsCard->uiResponseBits))
	{
		uiByte = mpsCard->auiResponse[mpsCard->uiResponseSent / 8];
		uiBit = (uint8)((uiByte >> (7 - (mpsCard->uiResponseSent % 8))) & 1);
		if (!uiBit)
		{
			uiLines &= ~HCP65_SD_GPIO_CMD;
		}
	}
	if (mpsCard->uiDAT0Sent < mpsCard->uiNumDAT0Bits)
	{
		if (!mpsCard->auiDAT0Bits[mpsCard->uiDAT0Sent])
		{
			uiLines &= ~HCP65_SD_GPIO_DAT0;
		}
	}
	return uiLines;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65SDCard::ReadRegister(uint32 uiRegister)
{
	uint8	uiDriven;
	uint8	uiLines;
	uint8	uiValue;

	switch (uiRegister)
	{
	case HCP65_SD_GPIO:
		uiLines = CardDrivenLines();
		if (mbEnabled)
		{
			uiDriven = (uint8)(muiGPIO >> 4);
			uiLines = (uint8)((uiLines & ~uiDriven) | (muiGPIO & uiDriven));
		}
		return (uint8)(uiLines & 0x0F);

	case HCP65_SD_FLAGS:
		uiValue = (uint8)(muiFlags & 0x3F);
		if (mbEnabled)
		{
			uiValue |= HCP65_SD_FLAG_ENABLE;
		}
		if (mbInterrupt)
		{
			uiValue |= HCP65_SD_FLAG_INTERRUPT;
		}
		return uiValue;
	}
	return 0xFF;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::WriteRegister(uint32 uiRegister, uint8 uiValue)
{
	switch (uiRegister)
	{
	case HCP65_SD_ADDRESS_LOW:
		muiAddress = (muiAddress & 0xFFFF00) | uiValue;
		break;
	case HCP65_SD_ADDRESS_HIGH:
		muiAddress = (muiAddress & 0xFF00FF) | ((uint32)uiValue << 8);
		break;
	case HCP65_SD_ADDRESS_BANK:
		muiAddress = (muiAddress & 0x00FFFF) | ((uint32)uiValue << 16);
		break;
	case HCP65_SD_GPIO:
		WriteGPIO(uiValue);
		break;
	case HCP65_SD_DATA_COUNT_LOW:
		muiDataCount = (uint16)((muiDataCount & 0xF00) | uiValue);
		break;
	case HCP65_SD_DATA_COUNT_HIGH:
		muiDataCount = (uint16)((muiDataCount & 0x0FF) | ((uiValue & 0x0F) << 8));
		break;
	case HCP65_SD_CRC_COUNT_LOW:
		muiCRCCount = (uint16)((muiCRCCount & 0xF00) | uiValue);
		break;
	case HCP65_SD_CRC_COUNT_HIGH:
		muiCRCCount = (uint16)((muiCRCCount & 0x0FF) | ((uiValue & 0x0F) << 8));
		break;
	case HCP65_SD_CLEAR_INTERRUPT:
		mbInterrupt = false;
		break;
	case HCP65_SD_READ_START:
		StartRead();
		break;
	case HCP65_SD_FLAGS:
		WriteFlags(uiValue);
		break;
	case HCP65_SD_WRITE_DATA_LOW:
	case HCP65_SD_WRITE_DATA_HIGH:
		WriteStream(uiValue);
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// D6 enables the board (the bus switch to the selected socket and the GPIO
// drivers); it can't while the board is held in reset.  Disabling stops a
// read.  The interrupt is cleared while interrupts are disabled.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::WriteFlags(uint8 uiValue)
{
	bool	bPreviousCLK;

	bPreviousCLK = (ReadRegister(HCP65_SD_GPIO) & HCP65_SD_GPIO_CLK) != 0;
	muiFlags = (uint8)(uiValue & ~HCP65_SD_FLAG_ENABLE);
	mpsCard = &masSockets[muiFlags & HCP65_SD_FLAG_CARD];
	mbEnabled = ((uiValue & HCP65_SD_FLAG_ENABLE) != 0) && !mbHeldInReset;
	if (!mbEnabled)
	{
		mbReading = false;
	}
	if (!mbEnabled || !(muiFlags & HCP65_SD_FLAG_INTERRUPT))
	{
		mbInterrupt = false;
	}
	mbPreviousCLK = bPreviousCLK;
	WriteGPIO(muiGPIO);
}


//////////////////////////////////////////////////////////////////////////
//
// The card sees a clock edge when the level on SD CLK changes.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::WriteGPIO(uint8 uiValue)
{
	bool	bCLK;

	muiGPIO = uiValue;
	bCLK = (ReadRegister(HCP65_SD_GPIO) & HCP65_SD_GPIO_CLK) != 0;
	if (bCLK != mbPreviousCLK)
	{
		mbPreviousCLK = bCLK;
		if (HasCard())
		{
			if (bCLK)
			{
				RisingEdge();
			}
			else
			{
				FallingEdge();
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The card samples CMD.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::RisingEdge(void)
{
	bool	bCMD;

	if ((mpsCard->uiResponseBits != 0) && (mpsCard->uiResponseSent < mpsCard->uiResponseBits))
	{
		//Busy sending; the card doesn't listen while it responds.
		return;
	}
	bCMD = (ReadRegister(HCP65_SD_GPIO) & HCP65_SD_GPIO_CMD) != 0;
	ReceiveCommandBit(bCMD);
}


//////////////////////////////////////////////////////////////////////////
//
// The card moves its outputs on.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::FallingEdge(void)
{
	if (mpsCard->uiResponseBits != 0)
	{
		if (mpsCard->uiResponseDelay > 0)
		{
			mpsCard->uiResponseDelay--;
		}
		else if (mpsCard->uiResponseSent < mpsCard->uiResponseBits)
		{
			mpsCard->uiResponseSent++;
		}
	}
	if (mpsCard->uiDAT0Sent < mpsCard->uiNumDAT0Bits)
	{
		mpsCard->uiDAT0Sent++;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Commands are 48 bits: 0, 1, the 6 bit index, the 32 bit argument, CRC7
// and 1.  The card waits for a start bit, and ignores a command whose
// framing or CRC is wrong.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::ReceiveCommandBit(bool bBit)
{
	uint8	uiIndex;
	uint32	uiArgument;

	if ((mpsCard->uiCommandBits == 0) && bBit)
	{
		return;
	}
	if (mpsCard->uiCommandBits % 8 == 0)
	{
		mpsCard->auiCommand[mpsCard->uiCommandBits / 8] = 0;
	}
	if (bBit)
	{
		mpsCard->auiCommand[mpsCard->uiCommandBits / 8] |= (uint8)(0x80 >> (mpsCard->uiCommandBits % 8));
	}
	mpsCard->uiCommandBits++;
	if (mpsCard->uiCommandBits < 48)
	{
		return;
	}
	mpsCard->uiCommandBits = 0;

	if (((mpsCard->auiCommand[0] & 0xC0) != 0x40) || ((mpsCard->auiCommand[5] & 1) != 1))
	{
		return;
	}
	if ((mpsCard->auiCommand[5] >> 1) != CRC7(mpsCard->auiCommand, 5))
	{
		mpsCard->uiStatus |= HCP65_SD_COM_CRC_ERROR;
		return;
	}

	uiIndex = (uint8)(mpsCard->auiCommand[0] & 0x3F);
	uiArgument = ((uint32)mpsCard->auiCommand[1] << 24) | ((uint32)mpsCard->auiCommand[2] << 16) | ((uint32)mpsCard->auiCommand[3] << 8) | mpsCard->auiCommand[4];
	if (mpsCard->bApplicationCommand)
	{
		mpsCard->bApplicationCommand = false;
		ApplicationCommand(uiIndex, uiArgument);
	}
	else
	{
		Command(uiIndex, uiArgument);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::Command(uint8 uiIndex, uint32 uiArgument)
{
	uint8	auiCSD[16];
	uint8	auiCID[16];
	uint32	uiSize;

	switch (uiIndex)
	{
	case 0:
		mpsCard->eState = HSDCS_Idle;
		mpsCard->uiRCA = 0;
		mpsCard->bWideBus = false;
		mpsCard->bReadPending = false;
		mpsCard->bReadMultiple = false;
		mpsCard->bWritePending = false;
		mpsCard->uiNumDAT0Bits = 0;
		return;

	case 8:
		//Interface condition: echo the voltage and check pattern.
		if (mpsCard->eState == HSDCS_Idle)
		{
			Respond48(8, uiArgument & 0xFFF, true);
		}
		return;

	case 55:
		RespondR1(55);
		mpsCard->bApplicationCommand = true;
		return;

	case 2:
		if (mpsCard->eState == HSDCS_Ready)
		{
			memcpy(auiCID, gauiHCP65SDCID, 15);
			auiCID[15] = (uint8)((CRC7(auiCID, 15) << 1) | 1);
			RespondR2(auiCID);
			mpsCard->eState = HSDCS_Identification;
		}
		return;

	case 3:
		if ((mpsCard->eState == HSDCS_Identification) || (mpsCard->eState == HSDCS_StandBy))
		{
			mpsCard->uiRCA = HCP65_SD_RCA;
			mpsCard->eState = HSDCS_StandBy;
			Respond48(3, ((uint32)mpsCard->uiRCA << 16) | ((StateBits() >> 0) & 0x1FFF), true);
		}
		return;

	case 9:
		if ((mpsCard->eState == HSDCS_StandBy) && ((uiArgument >> 16) == mpsCard->uiRCA))
		{
			//CSD version 2: C_SIZE is the capacity in 512KB units, less one.
			memset(auiCSD, 0, sizeof(auiCSD));
			uiSize = (mpsCard->uiNumSectors / 1024) - 1;
			auiCSD[0] = 0x40;
			auiCSD[1] = 0x0E;
			auiCSD[3] = 0x32;
			auiCSD[4] = 0x5B;
			auiCSD[5] = 0x59;
			auiCSD[7] = (uint8)((uiSize >> 16) & 0x3F);
			auiCSD[8] = (uint8)(uiSize >> 8);
			auiCSD[9] = (uint8)uiSize;
			auiCSD[10] = 0x7F;
			auiCSD[11] = 0x80;
			auiCSD[12] = 0x0A;
			auiCSD[13] = 0x40;
			auiCSD[15] = (uint8)((CRC7(auiCSD, 15) << 1) | 1);
			RespondR2(auiCSD);
		}
		return;

	case 7:
		if ((uiArgument >> 16) == mpsCard->uiRCA)
		{
			RespondR1(7);
			mpsCard->eState = HSDCS_Transfer;
		}
		else
		{
			mpsCard->eState = HSDCS_StandBy;
		}
		return;

	case 13:
		if ((uiArgument >> 16) == mpsCard->uiRCA)
		{
			RespondR1(13);
		}
		return;

	case 16:
		if (mpsCard->eState == HSDCS_Transfer)
		{
			RespondR1(16);
		}
		return;

	case 12:
		if ((mpsCard->eState == HSDCS_SendingData) || (mpsCard->eState == HSDCS_ReceivingData))
		{
			mpsCard->bReadPending = false;
			mpsCard->bReadMultiple = false;
			mpsCard->bWritePending = false;
			mpsCard->eState = HSDCS_Transfer;
		}
		RespondR1(12);
		return;

	case 17:
	case 18:
	case 24:
		if (mpsCard->eState != HSDCS_Transfer)
		{
			mpsCard->uiStatus |= HCP65_SD_ILLEGAL_COMMAND;
			return;
		}
		if (uiArgument >= mpsCard->uiNumSectors)
		{
			mpsCard->uiStatus |= HCP65_SD_OUT_OF_RANGE;
			RespondR1(uiIndex);
			return;
		}
		RespondR1(uiIndex);
		if ((uiIndex == 17) || (uiIndex == 18))
		{
			mpsCard->uiReadSector = uiArgument;
			mpsCard->bReadPending = true;
			mpsCard->bReadMultiple = uiIndex == 18;
			mpsCard->eState = HSDCS_SendingData;
		}
		else
		{
			mpsCard->uiWriteSector = uiArgument;
			mpsCard->bWritePending = true;
			muiNumWriteNybbles = 0;
			mpsCard->eState = HSDCS_ReceivingData;
		}
		return;
	}

	mpsCard->uiStatus |= HCP65_SD_ILLEGAL_COMMAND;
}


//////////////////////////////////////////////////////////////////////////
//
// After CMD55.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::ApplicationCommand(uint8 uiIndex, uint32 uiArgument)
{
	switch (uiIndex)
	{
	case 41:
		//The first ACMD41 finds the card still busy initialising; after that it's ready.  The OCR: bit 31 ready,
		//bit 30 high capacity, 2.7V to 3.6V.  R3 has no CRC.
		if (mpsCard->eState == HSDCS_Idle)
		{
			Respond48(0x3F, 0x40FF8000, false);
			mpsCard->eState = HSDCS_Ready;
		}
		else if (mpsCard->eState == HSDCS_Ready)
		{
			Respond48(0x3F, 0xC0FF8000, false);
		}
		return;

	case 6:
		if (mpsCard->eState == HSDCS_Transfer)
		{
			mpsCard->bWideBus = (uiArgument & 3) == 2;
			RespondR1(6);
		}
		return;
	}
	Command(uiIndex, uiArgument);
}


//////////////////////////////////////////////////////////////////////////
//
// The card's state, ready for data and app command bits.
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65SDCard::StateBits(void)
{
	uint32	uiState;

	switch (mpsCard->eState)
	{
	case HSDCS_Idle:			uiState = 0;	break;
	case HSDCS_Ready:			uiState = 1;	break;
	case HSDCS_Identification:	uiState = 2;	break;
	case HSDCS_StandBy:			uiState = 3;	break;
	case HSDCS_Transfer:		uiState = 4;	break;
	case HSDCS_SendingData:		uiState = 5;	break;
	default:					uiState = 6;	break;
	}
	return (uiState << 9) | HCP65_SD_READY_FOR_DATA | (mpsCard->bApplicationCommand ? HCP65_SD_APP_CMD : 0);
}


//////////////////////////////////////////////////////////////////////////
//
// R1: the card status.  Its error bits are cleared by sending them.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::RespondR1(uint8 uiIndex)
{
	if (uiIndex == 55)
	{
		Respond48(uiIndex, mpsCard->uiStatus | StateBits() | HCP65_SD_APP_CMD, true);
	}
	else
	{
		Respond48(uiIndex, mpsCard->uiStatus | StateBits(), true);
	}
	mpsCard->uiStatus &= ~HCP65_SD_CLEARED_ON_READ;
}


//////////////////////////////////////////////////////////////////////////
//
// A 48 bit response: 0, 0, the 6 bit index, 32 bits of content, CRC7 (or
// all ones) and 1.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::Respond48(uint8 uiIndex, uint32 uiContent, bool bCRC)
{
	mpsCard->auiResponse[0] = (uint8)(uiIndex & 0x3F);
	mpsCard->auiResponse[1] = (uint8)(uiContent >> 24);
	mpsCard->auiResponse[2] = (uint8)(uiContent >> 16);
	mpsCard->auiResponse[3] = (uint8)(uiContent >> 8);
	mpsCard->auiResponse[4] = (uint8)uiContent;
	mpsCard->auiResponse[5] = bCRC ? (uint8)((CRC7(mpsCard->auiResponse, 5) << 1) | 1) : 0xFF;
	mpsCard->uiResponseBits = 48;
	mpsCard->uiResponseSent = 0;
	mpsCard->uiResponseDelay = HCP65_SD_RESPONSE_DELAY;
}


//////////////////////////////////////////////////////////////////////////
//
// A 136 bit response: 0, 0, 111111, then the CID or CSD's 127 bits and 1
// (its 16th byte is its CRC7 and end bit).
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::RespondR2(uint8* puiRegister)
{
	mpsCard->auiResponse[0] = 0x3F;
	memcpy(&mpsCard->auiResponse[1], puiRegister, 16);
	mpsCard->uiResponseBits = 136;
	mpsCard->uiResponseSent = 0;
	mpsCard->uiResponseDelay = HCP65_SD_RESPONSE_DELAY;
}


//////////////////////////////////////////////////////////////////////////
//
// Read Start: the board clocks the card until its data starts, stores Data
// Count bytes and clocks the CRC bytes through.  The card only sends a
// block after a read command; without one the board waits for ever.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::StartRead(void)
{
	uint64	uiClocks;

	mbInterrupt = false;
	if (!mbEnabled || !mpcHCP65)
	{
		return;
	}
	mbReading = true;
	mpsReading = mpsCard;
	if (HasCard() && mpsCard->bReadPending)
	{
		uiClocks = HCP65_SD_ACCESS_CLOCKS + 1 + 2 * ((uint64)muiDataCount + muiCRCCount) + 1;
		muiReadDone = mpcHCP65->NumCycles() + uiClocks;
	}
	else
	{
		muiReadDone = HCP65_NEVER;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The block is in main memory.  More than 512 bytes takes the CRC bytes and
// then the bus floating high.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::FinishRead(void)
{
	uint8*	puiMain;
	uint8*	puiSector;
	uint8	auiCRC[8];
	uint32	i;
	uint32	uiLine;
	uint8	auiBits[1024];
	uint16	uiCRC;
	uint8	uiByte;
	SHCP65SDCardSocket*	psCard;

	mbReading = false;
	psCard = mpsReading;
	mpsReading = NULL;
	if (!psCard || !psCard->puiImage)
	{
		return;
	}
	puiMain = mpcHCP65->GetMainMemory();
	puiSector = &psCard->puiImage[(size_t)psCard->uiReadSector * HCP65_SD_SECTOR_SIZE];

	//The CRC bytes as the card sends them: CRC16 of each DAT line, the lines' bits interleaved in nybbles.
	memset(auiCRC, 0, sizeof(auiCRC));
	for (uiLine = 0; uiLine < 4; uiLine++)
	{
		for (i = 0; i < 1024; i++)
		{
			uiByte = puiSector[i / 2];
			auiBits[i] = (uint8)((((i & 1) ? uiByte : (uiByte >> 4)) >> uiLine) & 1);
		}
		uiCRC = CRC16(auiBits, 1024);
		for (i = 0; i < 16; i++)
		{
			if (uiCRC & (0x8000 >> i))
			{
				auiCRC[i / 2] |= (uint8)(1 << (uiLine + ((i & 1) ? 0 : 4)));
			}
		}
	}

	for (i = 0; i < muiDataCount; i++)
	{
		if (i < HCP65_SD_SECTOR_SIZE)
		{
			uiByte = puiSector[i];
		}
		else if (i < HCP65_SD_SECTOR_SIZE + 8)
		{
			uiByte = auiCRC[i - HCP65_SD_SECTOR_SIZE];
		}
		else
		{
			uiByte = 0xFF;
		}
		puiMain[(muiAddress + i) & 0xFFFFFF] = uiByte;
	}
	muiAddress = (muiAddress + muiDataCount) & 0xFFFFFF;
	muiSectorsRead++;

	//CMD18 goes on to the next sector (until CMD12, or the card's end); CMD17's one block is done.
	psCard->uiReadSector++;
	if (!psCard->bReadMultiple || (psCard->uiReadSector >= psCard->uiNumSectors))
	{
		psCard->bReadPending = false;
		psCard->bReadMultiple = false;
		psCard->eState = HSDCS_Transfer;
	}
	if (muiFlags & HCP65_SD_FLAG_INTERRUPT)
	{
		mbInterrupt = true;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Each byte stored to Write Data is two nybbles on DAT3-DAT0, high first.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::WriteStream(uint8 uiValue)
{
	if (!HasCard() || !mpsCard->bWritePending)
	{
		return;
	}
	if (muiNumWriteNybbles < HCP65_SD_WRITE_NYBBLES)
	{
		muiWriteNybbles[muiNumWriteNybbles++] = (uint8)(uiValue >> 4);
	}
	if (muiNumWriteNybbles < HCP65_SD_WRITE_NYBBLES)
	{
		muiWriteNybbles[muiNumWriteNybbles++] = (uint8)(uiValue & 0xF);
	}
	if (muiNumWriteNybbles == HCP65_SD_WRITE_NYBBLES)
	{
		FinishWrite();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Checks the start and end nybbles and each line's CRC16, writes the
// sector, and gives DAT0's CRC status (0, 010 accepted or 101 CRC error,
// 1) two clocks later, then a few clocks of busy (0).
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::FinishWrite(void)
{
	uint8	auiBits[1024];
	uint32	uiLine;
	uint32	i;
	uint16	uiCRC;
	uint16	uiSent;
	bool	bGood;
	uint8*	puiSector;

	bGood = (muiWriteNybbles[0] == 0) && (muiWriteNybbles[HCP65_SD_WRITE_NYBBLES - 1] == 0xF);
	for (uiLine = 0; bGood && (uiLine < 4); uiLine++)
	{
		for (i = 0; i < 1024; i++)
		{
			auiBits[i] = (uint8)((muiWriteNybbles[1 + i] >> uiLine) & 1);
		}
		uiCRC = CRC16(auiBits, 1024);
		uiSent = 0;
		for (i = 0; i < 16; i++)
		{
			uiSent = (uint16)((uiSent << 1) | ((muiWriteNybbles[1025 + i] >> uiLine) & 1));
		}
		bGood = uiCRC == uiSent;
	}

	if (bGood)
	{
		puiSector = &mpsCard->puiImage[(size_t)mpsCard->uiWriteSector * HCP65_SD_SECTOR_SIZE];
		for (i = 0; i < HCP65_SD_SECTOR_SIZE; i++)
		{
			puiSector[i] = (uint8)((muiWriteNybbles[1 + i * 2] << 4) | muiWriteNybbles[2 + i * 2]);
		}
		muiSectorsWritten++;
	}

	//Two clocks high (Ncrc), then the status token.
	mpsCard->uiNumDAT0Bits = 0;
	mpsCard->auiDAT0Bits[mpsCard->uiNumDAT0Bits++] = 1;
	mpsCard->auiDAT0Bits[mpsCard->uiNumDAT0Bits++] = 1;
	mpsCard->auiDAT0Bits[mpsCard->uiNumDAT0Bits++] = 0;
	mpsCard->auiDAT0Bits[mpsCard->uiNumDAT0Bits++] = bGood ? 0 : 1;
	mpsCard->auiDAT0Bits[mpsCard->uiNumDAT0Bits++] = bGood ? 1 : 0;
	mpsCard->auiDAT0Bits[mpsCard->uiNumDAT0Bits++] = bGood ? 0 : 1;
	mpsCard->auiDAT0Bits[mpsCard->uiNumDAT0Bits++] = 1;
	for (i = 0; i < 4; i++)
	{
		mpsCard->auiDAT0Bits[mpsCard->uiNumDAT0Bits++] = 0;
	}
	mpsCard->uiDAT0Sent = 0;
	muiNumWriteNybbles = 0;
	mpsCard->bWritePending = false;
	mpsCard->eState = HSDCS_Transfer;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SDCard::CatchUp(uint64 uiCycle)
{
	if (mbReading && (muiReadDone != HCP65_NEVER) && (uiCycle >= muiReadDone))
	{
		FinishRead();
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65SDCard::NextEvent(void)
{
	if (mbReading)
	{
		return muiReadDone;
	}
	return HCP65_NEVER;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65SDCard::IsInterrupting(void)
{
	return mbInterrupt;
}


//////////////////////////////////////////////////////////////////////////
//
// x^7 + x^3 + 1, over whole bytes.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65SDCard::CRC7(uint8* puiData, uint32 uiLength)
{
	uint8	uiCRC;
	uint32	i;
	int		iBit;
	uint8	uiIn;

	uiCRC = 0;
	for (i = 0; i < uiLength; i++)
	{
		for (iBit = 7; iBit >= 0; iBit--)
		{
			uiIn = (uint8)(((puiData[i] >> iBit) & 1) ^ ((uiCRC >> 6) & 1));
			uiCRC = (uint8)((uiCRC << 1) & 0x7F);
			if (uiIn)
			{
				uiCRC ^= 0x09;
			}
		}
	}
	return uiCRC;
}


//////////////////////////////////////////////////////////////////////////
//
// x^16 + x^12 + x^5 + 1 (CCITT, starting at 0) over uiLength bits, one bit
// per byte.
//
//////////////////////////////////////////////////////////////////////////
uint16 CHCP65SDCard::CRC16(uint8* puiBits, uint32 uiLength)
{
	uint16	uiCRC;
	uint32	i;
	uint16	uiIn;

	uiCRC = 0;
	for (i = 0; i < uiLength; i++)
	{
		uiIn = (uint16)((puiBits[i] & 1) ^ (uiCRC >> 15));
		uiCRC = (uint16)(uiCRC << 1);
		if (uiIn)
		{
			uiCRC ^= 0x1021;
		}
	}
	return uiCRC;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8* CHCP65SDCard::GetImage(int iSocket)
{
	return ((iSocket >= 0) && (iSocket < HCP65_SD_NUM_SOCKETS)) ? masSockets[iSocket].puiImage : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65SDCard::NumSectors(int iSocket)
{
	return ((iSocket >= 0) && (iSocket < HCP65_SD_NUM_SOCKETS)) ? masSockets[iSocket].uiNumSectors : 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65SDCard::NumSectorsRead(void)
{
	return muiSectorsRead;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65SDCard::NumSectorsWritten(void)
{
	return muiSectorsWritten;
}
