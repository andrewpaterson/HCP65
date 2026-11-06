#ifndef __DEVICE_ROM_H__
#define __DEVICE_ROM_H__
#include "BaseLib/PrimitiveTypes.h"



//The device ROM: the first 1KB of an HCP65 device's slot describes the board (HCP65 Website docs/hcp65/devices.md, Device
//ROM).  The kernal reads the same layout (Kernal/KernalDevices.h).  Words are little endian.
#define DEVICE_ROM_SIZE					0x400
#define DEVICE_ROM_MAGIC				0xC0DAFE1A
#define DEVICE_ROM_FORMAT				1
#define DEVICE_ROM_MAGIC_OFFSET			0x000		//4 bytes.
#define DEVICE_ROM_FORMAT_OFFSET		0x004
#define DEVICE_ROM_REVISION_OFFSET		0x005
#define DEVICE_ROM_DEVICE_BUS_OFFSET	0x006		//DEVICE_BUS_*.
#define DEVICE_ROM_MAIN_BUS_OFFSET		0x007		//DEVICE_MAIN_BUS_*.
#define DEVICE_ROM_NAME_OFFSET			0x010
#define DEVICE_ROM_NAME_LENGTH			64			//Zero terminated and zero padded: the last byte is always 0.
#define DEVICE_ROM_MAKER_OFFSET			0x050
#define DEVICE_ROM_MAKER_LENGTH			64
#define DEVICE_ROM_DRIVER_OFFSET		0x090
#define DEVICE_ROM_DRIVER_LENGTH		32			//The driver's file in \DRIVERS\ on the SD card, "" for none.
#define DEVICE_ROM_COUNT_OFFSET			0x0B0		//2 bytes: the number of registers listed.
#define DEVICE_ROM_REGISTERS_OFFSET		0x0B2		//3 bytes each: the offset in the slot (2 bytes) then DEVICE_REGISTER_*.
#define DEVICE_ROM_REGISTER_SIZE		3
#define DEVICE_ROM_MAX_REGISTERS		281			//Up to the checksum.
#define DEVICE_ROM_CHECKSUM_OFFSET		0x3FF		//Makes the ROM's bytes sum to 0 (mod 256).

#define DEVICE_BUS_32KB					0
#define DEVICE_BUS_128KB				1

#define DEVICE_MAIN_BUS_MASTER			0x01		//Can drive the main bus (DMA).
#define DEVICE_MAIN_BUS_WINDOW_A		0x02		//Provides a 2MB memory window on the main bus: chip A.
#define DEVICE_MAIN_BUS_WINDOW_B		0x04		//Chip B.

#define DEVICE_REGISTER_READ			0
#define DEVICE_REGISTER_WRITE			1			//A register that is read and written is listed twice.


//A device's ROM as it is laid out in its flash: what the board is, what it needs of the buses and its registers.  The
//unused bytes are erased flash ($FF).  The register list is kept sorted by offset, a read before a write.
class CDeviceROM
{
protected:
	uint8	mauiROM[DEVICE_ROM_SIZE];

public:
	void	Init(const char* szName, const char* szManufacturer, uint8 uiRevision, uint8 uiDeviceBus, uint8 uiMainBus, const char* szDriverFile);
	bool	AddRegister(uint16 uiOffset, uint8 uiDirection);
	void	AddRegisters(uint16 uiOffset, uint16 uiCount, uint8 uiDirection);

	uint8	Read(uint32 uiOffset);
	uint16	NumRegisters(void);
	uint8*	GetROM(void);

protected:
	void	SetString(uint32 uiOffset, uint32 uiLength, const char* sz);
	void	SetChecksum(void);
};


#endif // __DEVICE_ROM_H__
