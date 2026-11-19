#ifndef __HCP65_BLITTER_BOARD_H__
#define __HCP65_BLITTER_BOARD_H__


//The HCP65 Blitter Board (HCP65Lib/HCP65Blitter.h has the board) and its driver, \OS\Drivers\BLITTER.DRV.  The board
//copies bytes from one place in main memory to another, leaving out those of one colour if asked, in two processor
//cycles a byte.
//
//A program doesn't use it directly, as it works on main memory where it really is, not where a program sees it: it
//has nothing to open.  The kernal gives it the copies that drawing on the screen makes (KernalScreenBlit, HCP65User.h)
//when the board is there, and makes them itself when it isn't: what is drawn is the same.
#define BLITTER_BOARD_MANUFACTURER		"HCP65"
#define BLITTER_BOARD_NAME				"Blitter Board"


#endif // __HCP65_BLITTER_BOARD_H__
