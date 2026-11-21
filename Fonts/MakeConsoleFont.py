#!/usr/bin/env python3
"""Makes CONSOLE.SFT, the HCP65 console's 8x16 font, and CONSOLE.bmp, a picture of it, from a PC Screen Font (.psf or
.psf.gz) with a Unicode table.

CONSOLE.SFT is an SFT container (CommonLibraries' SupportLib/SFTCommon.h, version SFT_VERSION) of 256 transparent
images of 8 by 16 and no palette, in Latin-1 order (image n is the character U+00n): what a blit draws.  A glyph's pixels are $FF, and the rest of it
isn't drawn.  Each row of a glyph is one run: a mixed run, whose gaps are transparent pixels, where it has gaps.  The control characters ($00-$1F and $7F to $9F) are blank, as is any character the font hasn't.  The
console loads it from the SD card's \\OS\\Fonts\\CONSOLE.SFT.

CONSOLE.bmp is the same glyphs as a 24 bit Windows bitmap of 128 by 256, white on black: sixteen to a row, glyph n at
(8 x (n mod 16), 16 x (n div 16)).  It is to look at, and for the tests to compare the SFT font with.

CONSOLE.SFT in this directory was made from Terminus Font 16 (Lat15-Terminus16.psf.gz, as Debian's console-setup has
it), which is under the SIL Open Font License 1.1: Terminus-OFL.txt beside it.

    MakeConsoleFont.py [font.psf.gz] [CONSOLE.SFT] [CONSOLE.bmp]
"""
import gzip
import struct
import sys

GLYPH_WIDTH = 8
GLYPH_HEIGHT = 16
NUM_GLYPHS = 256
DEFAULT_FONT = "/usr/share/consolefonts/Lat15-Terminus16.psf.gz"
DEFAULT_OUTPUT = "CONSOLE.SFT"
DEFAULT_BITMAP_OUTPUT = "CONSOLE.bmp"
BITMAP_GLYPHS_ACROSS = 16

SFT_MAGIC = 0x4653
SFT_VERSION = 1
SFT_TYPE_TRANSPARENT = 2
SFT_TYPE_CONTAINER = 3
SFT_RUN_END = 0x00
SFT_RUN_SKIP = 0x80
SFT_RUN_ROWS = 0xC0
SFT_RUN_MIXED = 0xE0
SFT_RUN_MAX_COUNT = 0x40
SFT_RUN_MAX_ROWS = 0x20
SFT_PIXEL = 0xFF
SFT_TRANSPARENT = 0x00
MERGE_GAP = GLYPH_WIDTH     #A row's runs this near each other are one mixed run: 0 for none.
SFT_BLOCK = 0x10000


def ReadFont(szPath):
    """The font's glyphs (bytes, a row a byte) and the glyph of each code point it maps."""
    with open(szPath, "rb") as cFile:
        abData = cFile.read()
    if abData[:2] == b"\x1f\x8b":
        abData = gzip.decompress(abData)

    if abData[:2] == b"\x36\x04":
        #PSF 1: 256 or 512 glyphs 8 wide, the Unicode table 16 bit code points, $FFFF ending a glyph's.
        iMode, iHeight = abData[2], abData[3]
        iNumGlyphs = 512 if (iMode & 1) else 256
        iOffset = 4
        iGlyphSize = iHeight
        iWidth = 8
        aGlyphs = [abData[iOffset + i * iGlyphSize:iOffset + (i + 1) * iGlyphSize] for i in range(iNumGlyphs)]
        dMap = {}
        if iMode & 2:
            abTable = abData[iOffset + iNumGlyphs * iGlyphSize:]
            iGlyph = 0
            for i in range(0, len(abTable) - 1, 2):
                iCode = struct.unpack("<H", abTable[i:i + 2])[0]
                if iCode == 0xFFFF:
                    iGlyph += 1
                    if iGlyph >= iNumGlyphs:
                        break
                elif iCode != 0xFFFE:
                    dMap.setdefault(iCode, iGlyph)
        return iWidth, iHeight, aGlyphs, dMap

    if abData[:4] == b"\x72\xb5\x4a\x86":
        #PSF 2: the Unicode table UTF-8 sequences, $FF ending a glyph's.
        _, _, iHeaderSize, iFlags, iNumGlyphs, iGlyphSize, iHeight, iWidth = struct.unpack("<8I", abData[:32])
        aGlyphs = [abData[iHeaderSize + i * iGlyphSize:iHeaderSize + (i + 1) * iGlyphSize] for i in range(iNumGlyphs)]
        dMap = {}
        if iFlags & 1:
            abTable = abData[iHeaderSize + iNumGlyphs * iGlyphSize:]
            for iGlyph, abEntry in enumerate(abTable.split(b"\xff")[:iNumGlyphs]):
                for cChar in abEntry.split(b"\xfe")[0].decode("utf-8", "ignore"):
                    dMap.setdefault(ord(cChar), iGlyph)
        return iWidth, iHeight, aGlyphs, dMap

    raise ValueError(szPath + " isn't a PC Screen Font.")


def AddSFTCount(abImage, iCode, iCount, iMax):
    """iCount pixels skipped (SFT_RUN_SKIP, iMax SFT_RUN_MAX_COUNT) or rows moved down (SFT_RUN_ROWS,
    SFT_RUN_MAX_ROWS), iMax at a time."""
    while iCount > 0:
        iChunk = min(iCount, iMax)
        abImage.append(iCode | (iChunk - 1))
        iCount -= iChunk


def MakeSFTImage(abGlyph):
    """The glyph (a row a byte, its leftmost pixel bit 7) as a transparent SFT image.

    After its header (magic, version and type), width and height it is bytes: 1 to 127 is that many pixels, which follow; SFT_RUN_SKIP + (n - 1)
    skips n pixels across; SFT_RUN_ROWS + (n - 1) moves n rows down to the row's left; SFT_RUN_MIXED + (n - 1) is n
    pixels, which follow, of which those that are SFT_TRANSPARENT aren't drawn; SFT_RUN_END ends it.  A row that has
    pixels is reached by moving down from the last that had.

    A row's runs that are no more than MERGE_GAP pixels apart are one mixed run, so that a row is one copy for a
    blitter: with a gap of the glyph's width, every row is.
    """
    abImage = bytearray(struct.pack("<HBBHH", SFT_MAGIC, SFT_VERSION, SFT_TYPE_TRANSPARENT, GLYPH_WIDTH, GLYPH_HEIGHT))
    iRows = 0
    for iRow in range(GLYPH_HEIGHT):
        abPixels = [bool(abGlyph[iRow] & (0x80 >> iColumn)) for iColumn in range(GLYPH_WIDTH)]
        iSkip = 0
        iColumn = 0
        while iColumn < GLYPH_WIDTH:
            if not abPixels[iColumn]:
                iSkip += 1
                iColumn += 1
                continue

            #A run, and the runs after it that are near enough to be one with it.
            iStart = iColumn
            iEnd = iStart
            bMixed = False
            while True:
                while (iEnd < GLYPH_WIDTH) and abPixels[iEnd]:
                    iEnd += 1
                iNext = iEnd
                while (iNext < GLYPH_WIDTH) and not abPixels[iNext]:
                    iNext += 1
                if (iNext == GLYPH_WIDTH) or (iNext - iEnd > MERGE_GAP):
                    break
                bMixed = True
                iEnd = iNext

            AddSFTCount(abImage, SFT_RUN_ROWS, iRows, SFT_RUN_MAX_ROWS)
            AddSFTCount(abImage, SFT_RUN_SKIP, iSkip, SFT_RUN_MAX_COUNT)
            iLength = iEnd - iStart
            abImage.append((SFT_RUN_MIXED | (iLength - 1)) if bMixed else iLength)
            abImage += bytes(SFT_PIXEL if abPixels[i] else SFT_TRANSPARENT for i in range(iStart, iEnd))
            iRows = 0
            iSkip = 0
            iColumn = iEnd
        iRows += 1
    abImage.append(SFT_RUN_END)
    return bytes(abImage)


def MakeSFTContainer(aabImages):
    """The images as an SFT container: its header and count, then each image after the distance from that word to the
    next image's (0 for the last).  An image doesn't cross a 64KB boundary of the file: it starts at the next instead.
    """
    abFile = bytearray(struct.pack("<HBBH", SFT_MAGIC, SFT_VERSION, SFT_TYPE_CONTAINER, len(aabImages)))
    iPrevious = None
    for abImage in aabImages:
        iPosition = len(abFile)
        if (iPosition // SFT_BLOCK) != ((iPosition + len(abImage)) // SFT_BLOCK):
            iPosition = (iPosition // SFT_BLOCK + 1) * SFT_BLOCK
            abFile += bytes(iPosition - len(abFile))
        if iPrevious is not None:
            struct.pack_into("<H", abFile, iPrevious, iPosition - iPrevious)
        iPrevious = iPosition
        abFile += struct.pack("<H", 0) + abImage
    return bytes(abFile)


def MakeBitmap(aabGlyphs):
    """The glyphs as a 24 bit Windows bitmap, BITMAP_GLYPHS_ACROSS to a row, white on black."""
    iWidth = BITMAP_GLYPHS_ACROSS * GLYPH_WIDTH
    iHeight = (len(aabGlyphs) // BITMAP_GLYPHS_ACROSS) * GLYPH_HEIGHT
    iRowSize = (iWidth * 3 + 3) & ~3
    abPixels = bytearray()
    for y in range(iHeight - 1, -1, -1):    #A bitmap's rows are bottom first.
        abRow = bytearray()
        for x in range(iWidth):
            abGlyph = aabGlyphs[(y // GLYPH_HEIGHT) * BITMAP_GLYPHS_ACROSS + x // GLYPH_WIDTH]
            bPixel = bool(abGlyph[y % GLYPH_HEIGHT] & (0x80 >> (x % GLYPH_WIDTH)))
            abRow += b"\xff\xff\xff" if bPixel else b"\x00\x00\x00"
        abPixels += abRow + bytes(iRowSize - len(abRow))
    abHeader = struct.pack("<2sIHHI", b"BM", 54 + len(abPixels), 0, 0, 54)
    abInfo = struct.pack("<IiiHHIIiiII", 40, iWidth, iHeight, 1, 24, 0, len(abPixels), 2835, 2835, 0, 0)
    return abHeader + abInfo + bytes(abPixels)


def IsControl(iCode):
    return (iCode < 0x20) or ((iCode >= 0x7F) and (iCode < 0xA0))


def main(aszArguments):
    szFont = aszArguments[1] if len(aszArguments) > 1 else DEFAULT_FONT
    szOutput = aszArguments[2] if len(aszArguments) > 2 else DEFAULT_OUTPUT
    szBitmapOutput = aszArguments[3] if len(aszArguments) > 3 else DEFAULT_BITMAP_OUTPUT
    iWidth, iHeight, aGlyphs, dMap = ReadFont(szFont)
    if (iWidth != GLYPH_WIDTH) or (iHeight != GLYPH_HEIGHT):
        raise ValueError("%s is %dx%d, not %dx%d." % (szFont, iWidth, iHeight, GLYPH_WIDTH, GLYPH_HEIGHT))

    #Each glyph a row a byte, its leftmost pixel bit 7.
    aabGlyphs = []
    aiMissing = []
    for iCode in range(NUM_GLYPHS):
        if IsControl(iCode):
            aabGlyphs.append(bytes(GLYPH_HEIGHT))
        elif iCode in dMap:
            aabGlyphs.append(aGlyphs[dMap[iCode]][:GLYPH_HEIGHT])
        else:
            aabGlyphs.append(bytes(GLYPH_HEIGHT))
            aiMissing.append(iCode)

    abSFT = MakeSFTContainer([MakeSFTImage(abGlyph) for abGlyph in aabGlyphs])
    with open(szOutput, "wb") as cFile:
        cFile.write(abSFT)
    print("%s: %d bytes from %s%s" % (szOutput, len(abSFT), szFont, (", blank for " + " ".join("$%02X" % i for i in aiMissing)) if aiMissing else ""))

    abBitmap = MakeBitmap(aabGlyphs)
    with open(szBitmapOutput, "wb") as cFile:
        cFile.write(abBitmap)
    print("%s: %d bytes" % (szBitmapOutput, len(abBitmap)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
