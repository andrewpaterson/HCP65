#ifndef __HCP65_GRAPHICS_BOARD_H__
#define __HCP65_GRAPHICS_BOARD_H__


//The HCP65 Graphics Board (HCP65Lib/HCP65Graphics.h has the board) and its driver, \OS\Drivers\GRAPHICS.DRV.  The
//board copies bytes in main memory as the Blitter Board does, but reads each from the source address plus an offset it
//looks up by the count, so a row of a picture can be drawn stretched, shrunk or backwards.
//
//A program doesn't use it directly, as it works on main memory where it really is, not where a program sees it: it
//has nothing to open.
#define GRAPHICS_BOARD_MANUFACTURER		"HCP65"
#define GRAPHICS_BOARD_NAME				"Graphics Board"


#endif // __HCP65_GRAPHICS_BOARD_H__
