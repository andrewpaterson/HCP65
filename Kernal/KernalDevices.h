#ifndef __KERNAL_DEVICES_H__
#define __KERNAL_DEVICES_H__
#include "BaseLib/PrimitiveTypes.h"
#include "DeviceRomLib/DeviceROM.h"


#define KERNAL_NUM_DEVICE_SLOTS		20		//0 is the processor board's registers; 8 to 15 don't exist.
#define KERNAL_REGISTERS			0x100000
#define KERNAL_SMALL_SLOT_SIZE		0x8000
#define KERNAL_LARGE_SLOT_SIZE		0x20000

//The device ROM: the first 1KB of a device's slot describes the board (Website docs/hcp65/devices.md, Device ROM).  Its
//layout is DeviceRomLib's (DEVICE_ROM_*), which the emulator's boards build theirs with.  An empty socket's bus is pulled
//high, so it reads $FF.
#define KERNAL_DEVICE_ROM_EMPTY			0xFFFFFFFF	//The magic as an empty socket reads it.
#define KERNAL_DEVICE_MANUFACTURER		"HCP65"		//The HCP65's own boards.

//What a scan found in a socket.
#define KERNAL_DEVICE_NONE				0			//No socket (slots 0 and 8 to 15) or an empty one.
#define KERNAL_DEVICE_PRESENT			1
#define KERNAL_DEVICE_UNRECOGNISED		2			//Its ROM isn't a device ROM: wrong magic, format or checksum, or bad fields.
#define KERNAL_DEVICE_WRONG_SOCKET		3			//It needs a 128KB socket (16 to 19).


//The fixed part of a device ROM, as it is in the device's slot (at the DEVICE_ROM_*_OFFSETs).  Every field is at its
//natural alignment.
struct SKernalDeviceROM
{
	uint32	uiMagic;									//$000
	uint8	uiFormat;									//$004
	uint8	uiRevision;									//$005
	uint8	uiDeviceBus;								//$006  DEVICE_BUS_*.
	uint8	uiMainBus;									//$007  DEVICE_MAIN_BUS_*.
	uint8	auiReserved[8];								//$008
	char	szName[DEVICE_ROM_NAME_LENGTH];				//$010
	char	szManufacturer[DEVICE_ROM_MAKER_LENGTH];	//$050
	char	szDriverFile[DEVICE_ROM_DRIVER_LENGTH];		//$090
	uint16	uiNumRegisters;								//$0B0  The list follows at DEVICE_ROM_REGISTERS_OFFSET.
};


//A device socket and what was found in it.
struct SKernalDevice
{
	uint32				uiAddress;	//The slot's first address, 0 if the slot doesn't exist.
	uint32				uiSize;
	uint16				uiState;	//KERNAL_DEVICE_*.
	SKernalDeviceROM*	psROM;		//The device's ROM (at uiAddress), NULL unless the device is present.
};


extern SKernalDevice	gasKernalDevices[KERNAL_NUM_DEVICE_SLOTS];


void			KernalScanDevices(void);
SKernalDevice*	KernalFindDevice(const char* szManufacturer, const char* szName);
uint8			KernalReadRegister(uint8 uiRegister);
void			KernalWriteRegister(uint8 uiRegister, uint8 uiValue);
void			KernalEnableDevice(uint16 uiSlot);


#endif // __KERNAL_DEVICES_H__
