#ifndef __HCP65_VIDEO_BOARD_H__
#define __HCP65_VIDEO_BOARD_H__


//The HCP65 Video Board (HCP65Lib/HCP65Video.h has the board) through its driver, \OS\Drivers\VIDEO.DRV: open it with
//KernalDeviceOpen(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, index, unit, &handle) (HCP65User.h).  One program has the
//whole board while it has any unit open.
//
//The board has two frames: one is shown while the other is drawn.  Each pixel is a byte, an index into one of 128
//palettes of 256 colours; each pixel also has a palette number.  A mode (VIDEO_BOARD_SET_MODE) sets the timing, a
//row of pixels Width bytes long, one after another from 0, and turns the output on.  Nothing is shown until a mode is
//set.
//
//The units are the board's memories, read and written with KernalDeviceRead and KernalDeviceWrite at an offset:
//  VIDEO_BOARD_PIXELS: the frame being drawn, the pixel at (x, y) at y x Width + x.  While a palette number is set
//    (VIDEO_BOARD_SET_PALETTE, the default 0) writing a pixel also sets its palette number.
//  VIDEO_BOARD_PALETTE_NUMBERS: the frame being drawn's palette numbers, at the same offsets (D0-D6 are used).
//  VIDEO_BOARD_PALETTES: the colours: Red, then Green from VIDEO_BOARD_GREEN, then Blue from VIDEO_BOARD_BLUE, each
//    palette p's colour i at 256p + i.
//  VIDEO_BOARD_ROWS: the row table, each row's start address in its frame as 4 bytes (low, high, bank, unused).  A
//    mode sets it to one row after another; change it for scrolling, or rows shown twice.
//An offset or length past the end of a unit is KERNAL_DEVICE_BAD_ARGUMENT.
//
//VIDEO_BOARD_FLIP shows the frame that was being drawn, at the start of the next vertical blank, and the other one
//becomes the one drawn.  KernalDeviceWait with VIDEO_WAIT_VBLANK waits for the next vertical blank to start (the flip
//has happened by then).
#define VIDEO_BOARD_MANUFACTURER		"HCP65"
#define VIDEO_BOARD_NAME				"Video Board"

//Units.
#define VIDEO_BOARD_PIXELS				0
#define VIDEO_BOARD_PALETTE_NUMBERS		1
#define VIDEO_BOARD_PALETTES			2
#define VIDEO_BOARD_ROWS				3

#define VIDEO_BOARD_FRAME_SIZE			0x200000
#define VIDEO_BOARD_GREEN				0x8000		//In VIDEO_BOARD_PALETTES.
#define VIDEO_BOARD_BLUE				0x10000
#define VIDEO_BOARD_PALETTES_SIZE		0x18000
#define VIDEO_BOARD_ROWS_SIZE			0x1000		//1024 rows.
#define VIDEO_BOARD_NUM_PALETTES		128

//Modes.
#define VIDEO_MODE_640_480				0			//60Hz, 25.175MHz.
#define VIDEO_MODE_800_600				1			//60Hz, 40MHz.
#define VIDEO_MODE_1024_768				2			//60Hz, 65MHz.
#define VIDEO_NUM_MODES					3

//KernalDeviceControl codes.  The SETs return KERNAL_DEVICE_BAD_ARGUMENT for a value they don't take.
#define VIDEO_BOARD_SET_MODE			0		//VIDEO_MODE_*.
#define VIDEO_BOARD_GET_WIDTH			1		//The mode's, 0 before one is set.
#define VIDEO_BOARD_GET_HEIGHT			2
#define VIDEO_BOARD_SET_PALETTE			3		//0 to 127: the palette number a pixel write gives the pixel.  VIDEO_PALETTE_NONE: none.
#define VIDEO_BOARD_FLIP				4		//Value ignored.
#define VIDEO_BOARD_GET_SHOWN			5		//The frame shown, 0 or 1 (the other is drawn).
#define VIDEO_BOARD_GET_VBLANKS			6		//Vertical blanks since the driver started.
#define VIDEO_BOARD_OFF					7		//Stops the picture until a mode is set again.

#define VIDEO_PALETTE_NONE				0xFF

//KernalDeviceWait's value.
#define VIDEO_WAIT_VBLANK				0


#endif // __HCP65_VIDEO_BOARD_H__

