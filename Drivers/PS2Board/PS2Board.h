#ifndef __PS2_BOARD_H__
#define __PS2_BOARD_H__
#include "KernalDriver.h"
#include "UserLib/HCP65PS2Board.h"


//The board's registers, from its first ($400 in the slot).
#define PS2_FLAGS					0x00	//Flags & Interrupt.
#define PS2_FIFO					0x01	//Port n's (from 0) is PS2_FIFO + n.  R: its next byte.  W: clears its waiting.
#define PS2_DIRECT					0x03	//The GPIO on the lines.
#define PS2_STATE					0x04

#define PS2_FLAG_ENABLE				0x40
#define PS2_FLAG_INTERRUPT			0x80

#define PS2_DIRECT_DATA				0x01	//Port n's are shifted by 2n.
#define PS2_DIRECT_CLOCK			0x02
#define PS2_DIRECT_DRIVE			4		//A line's output enable is its level's bit shifted by this.

#define PS2_STATE_NO_PACKET			0x01	//0 while a packet is waiting.  Port n's are shifted by 4n.
#define PS2_STATE_READY				0x04	//The FIFO has a byte.
#define PS2_STATE_PORT_SHIFT		4

#define PS2_NUM_PORTS				2
#define PS2_RING_SIZE				256		//A ring holds one less (PS2_BOARD_BUFFER_SIZE); its uint8 indexes wrap on their own.
#define PS2_INHIBIT_READS			500		//Reads of Direct while Clock is held low before a request to send: over 100us.
#define PS2_QUIET_TICKS				2		//Clock high this long (3.9ms or more) and any packet has ended.
#define PS2_PACKET_TICKS			2		//The packet a send makes ends within this.


//One port's bytes: a ring in MPU RAM that the interrupt fills.  The device calls run with I set, so the interrupt only
//runs while one waits (KernalWaitForInterrupt) and the ring needs no locking.
struct SKernalPS2Port
{
	volatile uint8	uiHead;
	volatile uint8	uiTail;
	volatile uint16	uiOverruns;
	uint8			auiRing[PS2_RING_SIZE];
};


//The HCP65 PS2 Board's driver: its units are its two ports (PS2_BOARD_PORT_1 and PS2_BOARD_PORT_2), each opened by its
//own process (mbSeparateUnits).  The board is enabled with its interrupt on from Init, and the interrupt, which is
//trusted (it only touches the board and the rings), takes each port's bytes as their packets end.
//
//A byte is sent by hand through the GPIO: once the port has been quiet long enough for any packet to have ended (and its
//byte been taken), Clock is held low, Data taken low and Clock let go, and each bit is put on Data while the device
//holds Clock low.  The clocks make a packet of their own, whose byte is thrown away.
class CKernalPS2Board : public CKernalDriver
{
protected:
	SKernalPS2Port	masPorts[PS2_NUM_PORTS];
	uint8			muiDirect;		//What was last written to Direct.

public:
	uint16	Init(SKernalDevice* psDevice, uint16 uiSlot) override;
	void	Kill(void) override;

	uint16	NumUnits(void) override;
	void	Interrupt(void) override;

	uint16	Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone) override;
	uint16	Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone) override;
	uint16	Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult) override;
	uint16	Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks) override;

protected:
	uint16	OpenUnit(uint16 uiUnit) override;
	bool	IsWaitOver(uint16 uiUnit, uint32 uiValue) override;

	void	Take(uint16 uiPort, bool bDropFirst);
	void	Add(uint16 uiPort, uint8 uiByte);
	uint8	NumReceived(uint16 uiPort);
	void	Flush(uint16 uiPort);

	bool	Send(uint16 uiPort, uint8 uiByte);
	bool	SendBits(uint16 uiPort, uint16 uiFrame);
	void	WaitQuiet(uint16 uiPort);
	void	TakePacket(uint16 uiPort);
	bool	WaitClock(uint16 uiPort, bool bHigh, uint32 uiStart);
	bool	IsLineHigh(uint16 uiPort, uint8 uiLine);
	void	SetLow(uint16 uiPort, uint8 uiLine, bool bLow);
};


#define PS2_BOARD_MAX_BOARDS		2		//PS2 boards it drives.


extern "C" CKernalDriver*	KernalCreateDriver(uint16 uiIndex);


#endif // __PS2_BOARD_H__
