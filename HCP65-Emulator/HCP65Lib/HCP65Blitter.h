#ifndef __HCP65_BLITTER_H__
#define __HCP65_BLITTER_H__
#include "HCP65Device.h"


#define HCP65_BLITTER_NAME					"Blitter Board"
#define HCP65_BLITTER_DRIVER				"BLITTER.DRV"
#define HCP65_BLITTER_REGISTERS				0x400

//Registers, from $400.  They repeat every 32 bytes; $410-$41F decode nothing.
#define HCP65_BLITTER_FLAGS					0x00	//R/W.
#define HCP65_BLITTER_CLEAR_INTERRUPT		0x01	//W.
#define HCP65_BLITTER_TRANSPARENT_COLOUR	0x02	//R/W.
#define HCP65_BLITTER_DEST_LOW				0x04	//W: the destination address, bits 0-7.
#define HCP65_BLITTER_DEST_HIGH				0x05	//W: bits 8-15.
#define HCP65_BLITTER_DEST_BANK				0x06	//W: bits 16-23.
#define HCP65_BLITTER_START					0x07	//W: starts a copy.
#define HCP65_BLITTER_SOURCE_LOW			0x08	//W: the source address, bits 0-7.
#define HCP65_BLITTER_SOURCE_HIGH			0x09	//W: bits 8-15.
#define HCP65_BLITTER_SOURCE_BANK			0x0A	//W: bits 16-23.
#define HCP65_BLITTER_COUNT_START_LOW		0x0C	//W: the count a copy starts from.  R: the count, both bytes kept as
#define HCP65_BLITTER_COUNT_START_HIGH		0x0D	//the low one is read.
#define HCP65_BLITTER_COUNT_STOP_LOW		0x0E	//W: the count a copy stops at.
#define HCP65_BLITTER_COUNT_STOP_HIGH		0x0F	//W.

#define HCP65_BLITTER_FLAG_TRANSPARENCY		0x01	//Bytes that are the transparent colour aren't written.
#define HCP65_BLITTER_FLAG_ENABLE			0x40
#define HCP65_BLITTER_FLAG_INTERRUPT		0x80	//W: interrupt enable.  R: interrupt pending.

#define HCP65_BLITTER_CYCLES_PER_BYTE		2		//A byte is two Main CLKs, taken as the processor's clock: a read then a write.


//The HCP65 Blitter Board (Blitter sheets): a copy of bytes from one place in main memory to another, which the board
//does itself as the main bus's master.  It is a 32KB device.
//
//The board as it is meant to work (the schematic has faults: Kicad/Computer/Blitter/Blitter Board/Blitter Board
//Faults.txt):
//  - The source and destination addresses are 24 bit counters, written a byte at a time, and the count a 16 bit counter
//    with a start and a stop: Start loads the count with the start and copies a byte, stepping all three, until the
//    count is the stop.  That is stop - start bytes (mod 65536), none if they are the same.  The addresses are left just
//    past the last byte, so a second copy can carry on from them.
//  - With Flags D0 set, a byte that is the Transparent Colour is read but not written.
//  - A byte takes HCP65_BLITTER_CYCLES_PER_BYTE processor cycles.  The board has main memory until the copy is done
//    (CHCP65::SetMainMemoryBusy): the processor mustn't touch it.  The bytes are read and written as the processor would
//    (through the memory boards' windows; a watched chip, the Video Board's, is given the write), in order, so a
//    destination that overlaps the source from above repeats it.
//  - When the copy is done the interrupt is latched (Flags D7 reads it) if it is enabled, until Clear Interrupt, the next
//    Start, or the interrupt or the board is disabled.  Start does nothing while the board is disabled (Flags D6) or a
//    copy is under way, and disabling the board stops a copy where it is.
//  - A reset clears the flags, the addresses and the count; the count's start and stop and the transparent colour are
//    latches nothing resets.
class CHCP65BlitterBoard : public CHCP65Device
{
protected:
	CDeviceROM	mcROM;
	uint8		muiFlags;				//D0-D5 and D7 as written.
	bool		mbEnabled;
	bool		mbInterrupt;
	uint8		muiTransparentColour;
	uint32		muiSource;
	uint32		muiDest;
	uint16		muiCount;
	uint16		muiCountStart;
	uint16		muiCountStop;
	uint8		muiCountHighRead;		//The count's high byte when its low byte was last read.
	bool		mbCopying;
	uint64		muiCopyStart;			//The CPU cycle the copy's next byte starts.
	uint64		muiTotalCopied;			//Bytes copied by every copy.

public:
	void	Init(void);

	uint8	Read(uint32 uiOffset) override;
	void	Write(uint32 uiOffset, uint8 uiValue) override;
	void	Reset(void) override;

	void	CatchUp(uint64 uiCycle) override;
	uint64	NextEvent(void) override;
	bool	IsInterrupting(void) override;

	bool	IsCopying(void) const;
	uint32	GetSource(void) const;
	uint32	GetDest(void) const;
	uint16	GetCount(void) const;
	uint64	NumBytesCopied(void) const;

protected:
	uint8	ReadRegister(uint32 uiRegister);
	void	WriteRegister(uint32 uiRegister, uint8 uiValue);
	void	WriteFlags(uint8 uiValue);
	void	Start(void);
	uint64	CopyEnd(void) const;
	void	Finish(void);
};


#endif // __HCP65_BLITTER_H__
