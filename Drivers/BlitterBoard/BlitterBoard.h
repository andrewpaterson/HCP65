#ifndef __BLITTER_BOARD_H__
#define __BLITTER_BOARD_H__
#include "KernalDriver.h"
#include "UserLib/HCP65BlitterBoard.h"


//The board's registers, from its first ($400 in the slot).
#define BLITTER_FLAGS				0x00	//Flags & Interrupt.
#define BLITTER_CLEAR_INTERRUPT		0x01
#define BLITTER_TRANSPARENT_COLOUR	0x02
#define BLITTER_DEST				0x04	//Low, high and bank.
#define BLITTER_START				0x07
#define BLITTER_SOURCE				0x08	//Low, high and bank.
#define BLITTER_COUNT_START			0x0C	//Low and high.  Read: the count, its high byte kept as the low one is read.
#define BLITTER_COUNT_STOP			0x0E	//Low and high.

#define BLITTER_FLAG_TRANSPARENCY	0x01
#define BLITTER_FLAG_ENABLE			0x40
#define BLITTER_FLAG_INTERRUPT		0x80

#define BLITTER_BOARD_MAX_BOARDS	1		//Blitter boards it drives: the first is the kernal's.


//The HCP65 Blitter Board's driver.  It has no units a process can use: the board is the kernal's, which gives it the
//copies in main memory that it would make itself (KernalMainCopy, KernalDriver.h; the driver makes itself the kernal's
//copier in Init).
//
//A copy is the addresses, the count's stop (its start stays 0) and Start, then a wait for the count to reach the stop:
//the board has main memory until then, and the driver, which is in MPU RAM, reads only the board.  The interrupt isn't
//used.  The flags, the transparent colour and the stop are written for every copy, as the Video Board's driver gives the
//board its text's copies itself (gpuiKernalBlitter) and leaves them as it had them.
class CKernalBlitterBoard : public CKernalDriver
{
public:
	uint16	Init(SKernalDevice* psDevice, uint16 uiSlot) override;
	void	Kill(void) override;

	void	Copy(uint32 uiDest, uint32 uiSource, uint16 uiCount, uint16 uiTransparent);

protected:
	uint16	OpenUnit(uint16 uiUnit) override;
};


#endif // __BLITTER_BOARD_H__
