#ifndef __GRAPHICS_BOARD_H__
#define __GRAPHICS_BOARD_H__
#include "KernalDriver.h"
#include "UserLib/HCP65GraphicsBoard.h"


//The board's registers, from its first ($400 in the slot): the Blitter Board's, with a source address that can be read.
#define GRAPHICS_FLAGS				0x00	//Flags & Interrupt.
#define GRAPHICS_CLEAR_INTERRUPT	0x01
#define GRAPHICS_TRANSPARENT_COLOUR	0x02
#define GRAPHICS_DEST				0x04	//Low, high and bank.
#define GRAPHICS_START				0x07
#define GRAPHICS_SOURCE				0x08	//Low, high and bank.
#define GRAPHICS_COUNT_START		0x0C	//Low and high.  Read: the count, its high byte kept as the low one is read.
#define GRAPHICS_COUNT_STOP			0x0E	//Low and high.

#define GRAPHICS_FLAG_TRANSPARENCY	0x01
#define GRAPHICS_FLAG_ENABLE		0x40
#define GRAPHICS_FLAG_INTERRUPT		0x80

//The offsets, from the registers: KernalDriver.h's KERNAL_GRAPHICS_OFFSETS_LOW and _HIGH, which the Video Board's
//driver writes for its scaled blits.

#define GRAPHICS_BOARD_MAX_BOARDS	1		//Graphics boards it drives: the first is the kernal's.


//The HCP65 Graphics Board's driver.  It has no units a process can use: the board is the kernal's.  The driver enables
//it and gives the kernal its registers (gpuiKernalGraphics, KernalDriver.h), from which its offsets are found too.
//
//A copy is the offsets its counts need (written while no copy is under way), the Flags, the transparent colour, the
//addresses, the count's start and stop and Start, then a wait for the count to reach the stop: the board has main
//memory until then.  The interrupt isn't used.
class CKernalGraphicsBoard : public CKernalDriver
{
public:
	uint16	Init(SKernalDevice* psDevice, uint16 uiSlot) override;
	void	Kill(void) override;

protected:
	uint16	OpenUnit(uint16 uiUnit) override;
};


#endif // __GRAPHICS_BOARD_H__
