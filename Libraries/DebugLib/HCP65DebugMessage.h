#ifndef __HCP65_DEBUG_MESSAGE_H__
#define __HCP65_DEBUG_MESSAGE_H__
#include "BaseLib/PrimitiveTypes.h"


//How the kernal tells the emulator, for a debugger, what it does (Kernal/KernalDebug.h, HCP65Lib/HCP65DebugPort.h).  The
//kernal fills an SHCP65DebugMessage and executes WDM #HCP65_DEBUG_WDM_SIGNATURE with the message's address in A (bits 0
//to 15) and X (its bank).  A real W65C816 runs WDM as a two byte no-op, so a real HCP65 hears nothing; the emulator reads
//the message from memory.  The fields are little endian at the same offsets on the W65C816 and the host: each is at a
//multiple of its size, so neither compiler pads the struct.
#define HCP65_DEBUG_WDM_SIGNATURE	0xD6

#define HCP65_DEBUG_STARTED			1		//Process uiValue started running szName.
#define HCP65_DEBUG_ENDED			2		//Process uiValue ended with uiExitCode.
#define HCP65_DEBUG_BANK			3		//Main memory bank uiValue was given to a process.
#define HCP65_DEBUG_DRIVER			4		//The driver szName was put at uiAddress (uiSize bytes, relocated, not run yet)
											//for the board in slot uiValue.

#define HCP65_DEBUG_NAME_SIZE		64		//szName's bytes with its zero.


struct SHCP65DebugMessage
{
	uint8	uiCommand;					//HCP65_DEBUG_STARTED ... HCP65_DEBUG_DRIVER.
	uint8	uiValue;					//The process, the bank or the board's slot.
	uint16	uiExitCode;
	uint32	uiAddress;
	uint32	uiSize;
	char	szName[HCP65_DEBUG_NAME_SIZE];	//Zero terminated, cut short if it doesn't fit.
};


#endif // __HCP65_DEBUG_MESSAGE_H__
