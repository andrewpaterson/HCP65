#ifndef __HCP65_SD_CARD_H__
#define __HCP65_SD_CARD_H__
#include "BaseLib/Chars.h"
#include "HCP65Device.h"


#define HCP65_SD_NAME					"SD Card Board"
#define HCP65_SD_DRIVER					"SDCARD.DRV"
#define HCP65_SD_REGISTERS				0x400
#define HCP65_SD_NUM_SOCKETS			4

//Registers, from $400.  They repeat every 32 bytes; $410-$41F decode nothing.
#define HCP65_SD_ADDRESS_LOW			0x00	//W: DMA main memory address bits 0-7.
#define HCP65_SD_ADDRESS_HIGH			0x01	//W: bits 8-15.
#define HCP65_SD_ADDRESS_BANK			0x02	//W: bits 16-23.
#define HCP65_SD_GPIO					0x04	//W: D0-D3 levels, D4-D7 output enables.  R: D0-D3 pin levels.
#define HCP65_SD_DATA_COUNT_LOW			0x06	//W: bytes to store, bits 0-7.
#define HCP65_SD_DATA_COUNT_HIGH		0x07	//W: bits 8-11.
#define HCP65_SD_CRC_COUNT_LOW			0x08	//W: CRC bytes to clock through, bits 0-7.
#define HCP65_SD_CRC_COUNT_HIGH			0x09	//W: bits 8-11.
#define HCP65_SD_CLEAR_INTERRUPT		0x0A	//W.
#define HCP65_SD_READ_START				0x0B	//W: starts a block read.
#define HCP65_SD_FLAGS					0x0C	//R/W.
#define HCP65_SD_WRITE_DATA_LOW			0x0E	//W: one byte of the write stream (two nybbles, high first).
#define HCP65_SD_WRITE_DATA_HIGH		0x0F	//W: the same.

#define HCP65_SD_GPIO_CLK				0x01
#define HCP65_SD_GPIO_CMD				0x02
#define HCP65_SD_GPIO_DAT0				0x04
#define HCP65_SD_GPIO_DAT3				0x08

#define HCP65_SD_FLAG_CARD				0x03	//The socket, 0 to 3.
#define HCP65_SD_FLAG_ENABLE			0x40
#define HCP65_SD_FLAG_INTERRUPT			0x80	//W: interrupt enable.  R: interrupt pending.


enum EHCP65SDCardState
{
	HSDCS_Idle,
	HSDCS_Ready,
	HSDCS_Identification,
	HSDCS_StandBy,
	HSDCS_Transfer,
	HSDCS_SendingData,
	HSDCS_ReceivingData,
};


//A card in a socket: its image and its state.  The bus (CLK, CMD, DAT) only reaches the selected socket's card.
struct SHCP65SDCardSocket
{
	uint8*				puiImage;
	uint32				uiNumSectors;
	bool				bWriteBack;
	CChars				szFilename;
	EHCP65SDCardState	eState;
	bool				bApplicationCommand;
	uint16				uiRCA;
	uint32				uiStatus;			//The card status (R1's 32 bits).
	bool				bWideBus;
	uint8				auiCommand[6];
	uint8				uiCommandBits;
	uint8				auiResponse[17];
	uint8				uiResponseBits;		//Bits in auiResponse to send, including the start bit and end bit.
	uint8				uiResponseSent;
	uint8				uiResponseDelay;	//Clocks of CMD high before the response's start bit.
	uint32				uiReadSector;		//The sector the next block read sends.
	bool				bReadPending;
	bool				bReadMultiple;		//CMD18: blocks are sent until CMD12.
	uint32				uiWriteSector;
	bool				bWritePending;
	uint8				auiDAT0Bits[16];	//What the card drives on DAT0 after a write, one bit per falling edge.
	uint8				uiNumDAT0Bits;
	uint8				uiDAT0Sent;
};


//The HCP65 SD Card Board (SD Card sheets), with up to four SD cards (SDHC, block addressed) in its sockets, each an
//image file in memory.  Flags D0-D1 selects the socket the bus reaches.  It is a 32KB device.
//
//The board as it is meant to work (the schematics have faults, see the SD Card sheets' notes):
//  - Commands are bit banged through the GPIO register: SD CLK, CMD, DAT0 and DAT3.  The card samples CMD on SD CLK's
//    rising edge and changes its outputs (the CMD response, DAT0's CRC status and busy) on the falling edge.
//  - A block read is started by Read Start after the read command.  The board clocks the card and stores Data Count
//    bytes at the DMA address in main memory, clocks the CRC bytes through, then stops and latches its interrupt
//    (Flags D7).  Here the bytes are stored when the transfer ends, HCP65_SD_CLOCKS_PER_BYTE SD clocks per byte at
//    the CPU's clock rate after Read Start.
//  - A block write is a stream of bytes stored to Write Data: the start nybble (0), 1024 data nybbles, 16 CRC nybbles
//    (CRC16 of each DAT line) and the end nybble (F), high nybble first: 521 bytes.  Then the card's CRC status and
//    busy are clocked out on DAT0 through the GPIO register.
//The card checks the CRC7 of each command (and ignores a command with a bad one) and the CRC16s of written data.
class CHCP65SDCard : public CHCP65Device
{
protected:
	CDeviceROM		mcROM;

	//The board.
	uint8				muiFlags;
	bool				mbEnabled;
	bool				mbInterrupt;
	uint8				muiGPIO;
	uint32				muiAddress;
	uint16				muiDataCount;
	uint16				muiCRCCount;
	bool				mbReading;
	uint64				muiReadDone;		//The CPU cycle the board's read finishes.
	uint8				muiWriteNybbles[1042];
	uint16				muiNumWriteNybbles;
	bool				mbPreviousCLK;

	//The cards in the four sockets (Flags D0-D1 selects one: the bus switch connects only it).
	SHCP65SDCardSocket	masSockets[HCP65_SD_NUM_SOCKETS];
	SHCP65SDCardSocket*	mpsCard;			//The selected socket's.
	SHCP65SDCardSocket*	mpsReading;			//The card the board's read is from (while mbReading).
	uint32				muiSectorsRead;
	uint32				muiSectorsWritten;

public:
	void	Init(void);
	void	Kill(void) override;
	bool	LoadXML(CMarkupTag* pcTag, const char* szDirectory, CChars* pszError) override;

	bool	InsertCard(const char* szImageFilename, bool bWriteBack, int iSocket = 0);
	bool	InsertCard(uint8* puiImage, uint32 uiSize, int iSocket = 0);
	void	RemoveCard(int iSocket = 0);
	bool	Flush(void);

	uint8	Read(uint32 uiOffset) override;
	void	Write(uint32 uiOffset, uint8 uiValue) override;
	void	Reset(void) override;

	void	CatchUp(uint64 uiCycle) override;
	uint64	NextEvent(void) override;
	bool	IsInterrupting(void) override;

	uint8*	GetImage(int iSocket = 0);
	uint32	NumSectors(int iSocket = 0);
	uint32	NumSectorsRead(void);
	uint32	NumSectorsWritten(void);

	static uint8	CRC7(uint8* puiData, uint32 uiLength);
	static uint16	CRC16(uint8* puiBits, uint32 uiLength);

protected:
	uint8	ReadRegister(uint32 uiRegister);
	void	WriteRegister(uint32 uiRegister, uint8 uiValue);
	void	WriteFlags(uint8 uiValue);
	void	ResetCard(SHCP65SDCardSocket* psCard);
	bool	FlushCard(SHCP65SDCardSocket* psCard);
	void	WriteGPIO(uint8 uiValue);
	bool	HasCard(void);
	uint8	CardDrivenLines(void);
	void	RisingEdge(void);
	void	FallingEdge(void);
	void	ReceiveCommandBit(bool bBit);
	void	Command(uint8 uiIndex, uint32 uiArgument);
	void	ApplicationCommand(uint8 uiIndex, uint32 uiArgument);
	void	RespondR1(uint8 uiIndex);
	void	Respond48(uint8 uiIndex, uint32 uiContent, bool bCRC);
	void	RespondR2(uint8* puiRegister);
	void	StartRead(void);
	void	FinishRead(void);
	void	WriteStream(uint8 uiValue);
	void	FinishWrite(void);
	uint32	StateBits(void);
};


#endif // __HCP65_SD_CARD_H__
