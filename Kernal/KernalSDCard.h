#ifndef __KERNAL_SD_CARD_H__
#define __KERNAL_SD_CARD_H__
#include "BaseLib/FileDrive.h"
#include "KernalDriver.h"


#define KERNAL_SD_NUM_SOCKETS		4
#define KERNAL_SD_MAX_BOARDS		2		//The ROM's driver drives this many SD Card Boards.
#define KERNAL_SD_CARD_NAME			"SD Card Board"

#define KERNAL_SD_SUCCESS			0
#define KERNAL_SD_NO_CARD			1		//No response to CMD8.
#define KERNAL_SD_NOT_READY			2		//ACMD41 never finished initialising.
#define KERNAL_SD_NO_RESPONSE		3
#define KERNAL_SD_COMMAND_ERROR		4		//An R1 with an error bit set.
#define KERNAL_SD_READ_TIMEOUT		5
#define KERNAL_SD_WRITE_REJECTED	6
#define KERNAL_SD_OUT_OF_RANGE		7


//An SD card in one of the HCP65 SD Card Board's four sockets, used as a drive for a FAT volume.  Each access selects
//its socket first (Flags D0-D1).  Commands are bit banged through the board's GPIO register; sectors are read by the
//board's DMA straight into main memory (CMD18 for more than one), or into a bounce buffer in main memory and copied
//from there when the destination is MPU RAM (the board only reaches main memory); a sector is written as the 521 byte
//DAT stream (start nybble, data, the four lines' CRC16s, end nybble) through Write Data.  4 bit bus, block addressing
//(SDHC/SDXC).
class CKernalSDCard : public CFileDrive
{
protected:
	volatile uint8*	mpuiRegisters;
	uint8			muiFlags;
	uint16			muiRCA;
	uint8*			mpuiBounce;		//512 bytes in main memory, once the card is ready.
	uint8			mauiResponse[17];
	uint16			muiError;
	bool			mbIdentification;	//Until CMD3 the clock must stay under 400kHz: the C++ bit banging (a SetGPIO call per edge) is
										//used, not KernalSDCard.asm's, which clocks at about a MHz at 25MHz.

public:
	uint16	Init(volatile uint8* puiRegisters, uint8 uiSocket);
	void	Kill(void) override;

	bool	Read(uint64 uiSector, void* pvData) override;
	bool	Read(uint64 uiSector, uint32 uiNumSectors, void* pvData) override;
	bool	Write(uint64 uiSector, void* pvData) override;
	bool	Write(uint64 uiSector, uint32 uiNumSectors, void* pvData) override;
	bool	Erase(uint64 uiStartSector, uint64 uiStopSectorInclusive) override;

	uint16	GetError(void);

protected:
	void	Select(void);
	void	SetGPIO(uint8 uiValue);
	void	Clocks(uint16 uiClocks);
	void	SendCommand(uint8 uiIndex, uint32 uiArgument);
	bool	ReadResponse(uint16 uiBits);
	bool	Command(uint8 uiIndex, uint32 uiArgument, uint16 uiResponseBits);
	bool	CommandR1(uint8 uiIndex, uint32 uiArgument);
	uint32	ResponseContent(void);
	bool	ReadSectors(uint32 uiSector, uint32 uiNumSectors, uint8* puiData);
	bool	WriteSector(uint32 uiSector, void* pvData);
};


//The HCP65 SD Card Board's driver: the board's interrupt and its four card sockets (units 0 to 3), each card a drive.
//Only the kernal uses it (its volume): processes can't open it.
class CKernalSDCardBoard : public CKernalDriver
{
protected:
	CKernalSDCard	macCards[KERNAL_SD_NUM_SOCKETS];
	uint16			muiReadyCards;		//Bit n set: the card in socket n came up.

public:
	uint16			Init(SKernalDevice* psDevice, uint16 uiSlot) override;
	void			Kill(void) override;

	uint16			NumUnits(void) override;
	void			Interrupt(void) override;
	uint16			Open(uint16 uiUnit, uint8 uiProcess) override;

	CKernalSDCard*	GetCard(uint16 uiSocket);
	uint16			GetReadyCards(void);
};


uint8	KernalCRC7(uint8* puiData, uint16 uiLength);


#endif // __KERNAL_SD_CARD_H__
