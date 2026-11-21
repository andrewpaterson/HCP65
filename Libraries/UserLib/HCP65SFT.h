#ifndef __HCP65_SFT_H__
#define __HCP65_SFT_H__
#include "BaseLib/PrimitiveTypes.h"


//SFT images (CommonLibraries' SupportLib/SFTCommon.h reads and writes them): what a blit draws (HCP65User.h's
//KernalScreenBlit).  An SFT file is a record: an image, a palette or a container of them.  Every record starts with the
//same header: the magic number SFT_MAGIC ("SF"), the layout's version (SFT_VERSION) and its type (SFT_TYPE_*).  Every
//word is low byte first.
//
//A pixel is a byte: an index into the palette that comes before the image in its container, or with no palette a colour
//with red in D0-D2, green in D3-D5 and blue in D6-D7 (SFTColours' palette).
//
//An opaque image is an SSFTImage (SFT_TYPE_OPAQUE), then its rows from the top, each its width of pixels.
//
//A transparent image is an SSFTImage (SFT_TYPE_TRANSPARENT), then bytes that say where its pixels are, starting at
//its top left:
//  SFT_RUN_END              the image is done.
//  $01-$7F                  that many pixels follow: they are drawn from here to the right.
//  SFT_RUN_SKIP + (n - 1)   n pixels (1 to SFT_RUN_MAX_COUNT) are skipped to the right.
//  SFT_RUN_ROWS + (n - 1)   n rows (1 to SFT_RUN_MAX_ROWS) down, at the row's left.
//  SFT_RUN_MIXED + (n - 1)  n pixels (1 to SFT_RUN_MAX_MIXED) follow, of which those that are SFT_PIXEL_TRANSPARENT
//                           aren't drawn and the rest are, from here to the right: a run with short gaps in it, which
//                           a blitter draws as one copy that leaves that colour out.
//A longer run, skip or move down is several of them.  What isn't drawn stays as it was.
//
//A palette is an SSFTPalette (SFT_TYPE_PALETTE), then its uiCount reds, its uiCount greens and its uiCount blues: the
//colours of the indices uiFirst to uiFirst + uiCount - 1, no more than 256 of them (KernalScreenSetColours loads them
//into one of the Video Board's palettes).  Its uiHash is SFTPaletteHash's: palettes with the same hash are very likely
//the same colours at the same indices, so the images that use them can share one of the board's palettes.
//
//A container is an SSFTContainer, then its uiCount records (images and palettes), each after a word: how many bytes
//that word is before the next record's (0 for the last).  A palette is the palette of the images after it, up to the
//next.  No image crosses a 64KB boundary of the container's file.
#define SFT_MAGIC				0x4653
#define SFT_VERSION				1

#define SFT_TYPE_OPAQUE			1
#define SFT_TYPE_TRANSPARENT	2
#define SFT_TYPE_CONTAINER		3
#define SFT_TYPE_PALETTE		4

#define SFT_RUN_END				0x00
#define SFT_RUN_MAX_PIXELS		0x7F
#define SFT_RUN_SKIP			0x80
#define SFT_RUN_ROWS			0xC0
#define SFT_RUN_MIXED			0xE0
#define SFT_RUN_COUNT_MASK		0x3F
#define SFT_RUN_MAX_COUNT		0x40
#define SFT_RUN_ROWS_MASK		0x1F
#define SFT_RUN_MAX_ROWS		0x20
#define SFT_RUN_MIXED_MASK		0x1F
#define SFT_RUN_MAX_MIXED		0x20
#define SFT_PIXEL_TRANSPARENT	0x00

#define SFT_PALETTE_COLOURS		256


//What starts every record.
struct SSFTHeader
{
	uint16	uiMagic;
	uint8	uiVersion;
	uint8	uiType;
};


struct SSFTImage
{
	uint16	uiMagic;
	uint8	uiVersion;
	uint8	uiType;
	uint16	uiWidth;
	uint16	uiHeight;
};


struct SSFTContainer
{
	uint16	uiMagic;
	uint8	uiVersion;
	uint8	uiType;
	uint16	uiCount;
};


struct SSFTPalette
{
	uint16	uiMagic;
	uint8	uiVersion;
	uint8	uiType;
	uint16	uiFirst;
	uint16	uiCount;
	uint32	uiHash;
};


//SFTColours' palette: all 256 colours, and its hash.
#define SFT_COLOURS_SIZE		(sizeof(SSFTPalette) + 3 * SFT_PALETTE_COLOURS)
#define SFT_COLOURS_HASH		0x4A5B8702


//Whether the uiSize bytes at pv start with a record of this layout of the type (only its header is looked at).
extern "C" bool			SFTIsRecord(const void* pv, uint32 uiSize, uint8 uiType);

//The palette that shows an image's pixels as the colours they are with no palette: a palette record of SFT_COLOURS_SIZE
//bytes at pvPalette, its reds' D0-D2, greens' D3-D5 and blues' D6-D7 being the pixel's.
extern "C" void			SFTColours(void* pvPalette);

//FNV-1a of a palette's uiFirst and uiCount (each low byte first), then its reds, greens and blues (SupportLib's
//SFTPaletteHash).  The palette must be whole.
extern "C" uint32		SFTPaletteHash(const SSFTPalette* psPalette);

//The uiIndex'th image (from 0, its palettes not counted) of the container at pvContainer, which is uiSize bytes, and in
//*puiSize how many bytes it can be (up to the next record, or the container's end).  NULL if it isn't a container of
//this layout or hasn't that many images.
extern "C" const void*	SFTContainedImage(const void* pvContainer, uint32 uiSize, uint16 uiIndex, uint32* puiSize);

//The palette of the container's uiIndex'th image: the last palette before it.  NULL if there is none (its pixels are
//colours), or no such image.
extern "C" const SSFTPalette*	SFTContainedPalette(const void* pvContainer, uint32 uiSize, uint16 uiIndex);


#endif // __HCP65_SFT_H__
