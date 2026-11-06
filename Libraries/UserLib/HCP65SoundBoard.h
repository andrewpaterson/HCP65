#ifndef __HCP65_SOUND_BOARD_H__
#define __HCP65_SOUND_BOARD_H__


//The HCP65 Sound Board (HCP65Lib/HCP65Audio.h has the board) through its driver, \OS\Drivers\SOUND.DRV: open it
//with KernalDeviceOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, index, unit, &handle) (HCP65User.h).  Two 32KB
//sample RAMs played together through an 8 bit stereo DAC, RAM A on the left and RAM B on the right.
#define SOUND_BOARD_MANUFACTURER		"HCP65"
#define SOUND_BOARD_NAME				"Sound Board"

//Units: KernalDeviceWrite copies unsigned 8 bit samples to the unit's RAM at the offset, wrapping at its end.
#define SOUND_BOARD_LEFT				0		//RAM A.
#define SOUND_BOARD_RIGHT				1		//RAM B.
#define SOUND_BOARD_RAM_SIZE			0x8000

//KernalDeviceControl codes (either unit): SOUND_BOARD_SET_REGISTER + n writes register n with the value's low byte;
//SOUND_BOARD_GET_COUNTER gives the address of the samples the board plays next.
#define SOUND_BOARD_SET_REGISTER		0
#define SOUND_BOARD_NUM_REGISTERS		8
#define SOUND_BOARD_GET_COUNTER			8

//The registers.
#define SOUND_BOARD_FLAGS				0		//Read and write.
#define SOUND_BOARD_CLEAR_INTERRUPT		1		//Write.
#define SOUND_BOARD_START_LOW			2		//Write.  Read: counter low, which snapshots the counter.
#define SOUND_BOARD_START_HIGH			3		//Write.  Read: the snapshot's high byte.
#define SOUND_BOARD_END_LOW				4
#define SOUND_BOARD_END_HIGH			5
#define SOUND_BOARD_INTERRUPT_LOW		6
#define SOUND_BOARD_INTERRUPT_HIGH		7

#define SOUND_BOARD_FLAG_RATE			0x03	//0 = 32768Hz, 1 = 16384Hz, 2 = 8192Hz, 3 = 4096Hz.
#define SOUND_BOARD_FLAG_ENABLE			0x40	//Write 1 to enable, 0 to disable.  Read: enabled.
#define SOUND_BOARD_FLAG_INTERRUPT		0x80	//Write: interrupt enable.  Read: interrupt pending.

//KernalDeviceWait's value is the address (0 to $7FFF) the counter is waited for, sleeping until the board's
//interrupt.  It waits at most the ticks given (KERNAL_DEVICE_TIMEOUT): the counter may have passed it already, so
//look at the counter again.  SOUND_BOARD_WAIT_TICKS (7.8ms to 11.7ms) is enough.
#define SOUND_BOARD_WAIT_TICKS			2


#endif // __HCP65_SOUND_BOARD_H__
