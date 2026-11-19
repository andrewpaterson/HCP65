#ifndef __HCP65_LCD_BOARD_H__
#define __HCP65_LCD_BOARD_H__


//The HCP65 LCD and SNES Board (HCP65Lib/HCP65LCD.h has the board) through its driver, \OS\Drivers\LCD.DRV: open its
//display with KernalDeviceOpen(LCD_BOARD_MANUFACTURER, LCD_BOARD_NAME, index, LCD_BOARD_DISPLAY, &handle)
//(HCP65User.h).  The display is 16 characters on each of 2 lines.  The driver sets it up as it loads: on, no cursor,
//blank.  KERNAL_DEVICE_FAILED if the display never answered.
//
//KernalDeviceWrite puts the program's bytes on the display from the character the offset is: line 1 is offsets 0 to
//15, line 2 16 to 31, and offset plus length is at most LCD_BOARD_SIZE.  The bytes are the display's character codes:
//ASCII from $20 to $7D (but $5C is a yen sign), $7E and $7F arrows, katakana and symbols from $A1, and 0 to 7 the
//display's own eight characters (which the driver doesn't set yet).  KernalDeviceRead gives them back the same way.
//What is on the display stays after the program ends.
//
//The SNES controllers are units LCD_BOARD_SNES_1 and LCD_BOARD_SNES_2.  KernalDeviceRead from offset 0 gives a
//controller's buttons as a word, low byte first: a bit (LCD_BOARD_SNES_*) set for each button that is pressed.  The
//board reads both controllers all the time; one that isn't plugged in has nothing pressed.
#define LCD_BOARD_MANUFACTURER			"HCP65"
#define LCD_BOARD_NAME					"LCD and SNES Board"

//Units.
#define LCD_BOARD_DISPLAY				0
#define LCD_BOARD_SNES_1				1
#define LCD_BOARD_SNES_2				2
#define LCD_BOARD_NUM_UNITS				3

#define LCD_BOARD_COLUMNS				16
#define LCD_BOARD_LINES					2
#define LCD_BOARD_SIZE					(LCD_BOARD_COLUMNS * LCD_BOARD_LINES)

//A controller's buttons, in the order it sends them.
#define LCD_BOARD_SNES_B				0x0001
#define LCD_BOARD_SNES_Y				0x0002
#define LCD_BOARD_SNES_SELECT			0x0004
#define LCD_BOARD_SNES_START			0x0008
#define LCD_BOARD_SNES_UP				0x0010
#define LCD_BOARD_SNES_DOWN				0x0020
#define LCD_BOARD_SNES_LEFT				0x0040
#define LCD_BOARD_SNES_RIGHT			0x0080
#define LCD_BOARD_SNES_A				0x0100
#define LCD_BOARD_SNES_X				0x0200
#define LCD_BOARD_SNES_L				0x0400
#define LCD_BOARD_SNES_R				0x0800
#define LCD_BOARD_SNES_SIZE				2		//The bytes a controller's buttons are.

//KernalDeviceControl codes.
#define LCD_BOARD_CLEAR					1		//Every character a space.


#endif // __HCP65_LCD_BOARD_H__
