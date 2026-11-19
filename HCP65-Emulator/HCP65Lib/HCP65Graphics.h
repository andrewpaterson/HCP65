#ifndef __HCP65_GRAPHICS_H__
#define __HCP65_GRAPHICS_H__
#include "HCP65Blitter.h"


#define HCP65_GRAPHICS_NAME					"Graphics Board"
#define HCP65_GRAPHICS_DRIVER				"GRAPHICS.DRV"

//The registers are the Blitter Board's (HCP65Blitter.h), from $400, but that the source address can be read too and the
//count is 15 bits.
#define HCP65_GRAPHICS_OFFSETS_LOW			0x08000		//Device RAM: the low byte of each count's offset.
#define HCP65_GRAPHICS_OFFSETS_HIGH			0x10000		//Device RAM: the high byte.
#define HCP65_GRAPHICS_OFFSETS				0x8000		//Offsets, a byte of each in each RAM.
#define HCP65_GRAPHICS_COUNT_MASK			0x7FFF


//The HCP65 Graphics Board (Graphics sheets): a Blitter Board whose source bytes needn't be in order.  Each byte is read
//from the source address plus an offset, which is looked up by the count in a table the processor has written, and
//written to the destination address, which steps as the Blitter Board's does.  With the offsets 0, 1, 2... it is the
//Blitter Board's copy; with others the copy is the source backwards, stretched, shrunk or along any line through a
//picture.  It is a 128KB device.
//
//The board as it is meant to work (the schematic has faults: Kicad/Computer/Graphics/Graphics Board/Graphics Board
//Faults.txt).  What differs from the Blitter Board:
//  - The source address is a 24 bit register, which a copy doesn't change, a reset doesn't clear, and which can be read
//    and (as it is latches) written at any time.
//  - The offsets are 16 bit and unsigned, their low bytes in one 32KB RAM and their high bytes in another: the
//    processor has them as device RAM at $08000 and $10000 while no copy is under way.  During one they are the
//    board's: the processor reads $FF and its writes are lost.  A byte is read from the source address + the offset the
//    count has (24 bit: it carries into the bank).
//  - The count is 15 bits, as the tables are 32768 offsets: bit 15 of its start and stop is ignored, and it wraps from
//    32767 to 0.  A copy is (stop - start) mod 32768 bytes, its first byte read through offset `start`.
class CHCP65GraphicsBoard : public CHCP65BlitterBoard
{
protected:
	uint8*		mpuiOffsetsLow;
	uint8*		mpuiOffsetsHigh;

public:
	void	Init(void);
	void	Kill(void) override;

	uint8	Read(uint32 uiOffset) override;
	void	Write(uint32 uiOffset, uint8 uiValue) override;
	void	Reset(void) override;

	uint16	GetOffset(uint16 uiCount) const;
	void	SetOffset(uint16 uiCount, uint16 uiOffset);

protected:
	uint8	ReadRegister(uint32 uiRegister) override;
	void	WriteRegister(uint32 uiRegister, uint8 uiValue) override;
	uint8	ReadSource(void) override;
};


#endif // __HCP65_GRAPHICS_H__
