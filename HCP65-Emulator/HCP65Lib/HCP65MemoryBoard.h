#ifndef __HCP65_MEMORY_BOARD_H__
#define __HCP65_MEMORY_BOARD_H__
#include "HCP65Device.h"


#define HCP65_MEMORY_BOARD_NAME		"Memory Board"
#define HCP65_MEMORY_CHIP_SIZE		0x200000	//Each of a socket's two chips: 2MB (an IS61WV20488).
#define HCP65_MEMORY_WINDOW_SIZE	0x400000	//A socket's window: chip A the lower 2MB, chip B the upper.
#define HCP65_MEMORY_NUM_WINDOWS	4
#define HCP65_MEMORY_NUM_SOCKETS	8			//The channel's sockets, S to Z.
#define HCP65_MEMORY_CHANNEL_SLOT	1


//The HCP65 Memory Board (Memory Board and Memory 4MB sheets): a 4MB sub-board of two 2MB chips, and a device ROM
//saying it provides memory windows A and B.  Which 4MB of main memory it answers is the main memory channel's
//(CHCP65MemoryChannel): the HCP65 maps its chips there (GetMemoryChip).  Its memory is kept through resets.  The
//schematic has no Device Decode Stage A, so its ROM is DeviceROM/Boards/MemoryBoard.xml, written by hand.
class CHCP65MemoryBoard : public CHCP65Device
{
protected:
	CDeviceROM	mcROM;
	uint8*		mpuiMemory;		//Chip A then chip B.

public:
	void	Init(void);
	void	Kill(void) override;

	uint8	Read(uint32 uiOffset) override;
	void	Write(uint32 uiOffset, uint8 uiValue) override;
	uint8*	GetMemoryChip(int iChip) override;
};


//The main memory channel (device 1, the mainboard's Main Memory Channel and Main Memory Select sheets): two
//registers, written directly and repeating every 8 bytes through the slot, of a 2 bit window number for each memory
//socket.  Register 0 is sockets S, T, U (device 19) and V (device 18), register 1 W (device 4), X (device 3),
//Y (device 17) and Z (device 16), from D1-D0 up; S and T aren't wired on the small mainboard.  Nothing is read back
//(the bus floats to $FF).  A system reset clears them; holding the device in reset doesn't.
class CHCP65MemoryChannel : public CHCP65Device
{
protected:
	uint8	mauiRegisters[2];

public:
	void	Init(void);

	uint8	Read(uint32 uiOffset) override;
	void	Write(uint32 uiOffset, uint8 uiValue) override;
	void	Reset(void) override;
	void	SetHeldInReset(bool bHeld) override;

	int		GetSocketSlot(int iSocket);
	int		GetSocketWindow(int iSocket);
};


#endif // __HCP65_MEMORY_BOARD_H__
