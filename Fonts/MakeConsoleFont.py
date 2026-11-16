#!/usr/bin/env python3
"""Makes CONSOLE.FNT, the HCP65 console's 8x16 font, from a PC Screen Font (.psf or .psf.gz) with a Unicode table.

CONSOLE.FNT is 256 glyphs of 16 bytes, in Latin-1 order (glyph n is the character U+00n), each byte a row from the top
with its leftmost pixel in bit 7: 4096 bytes.  The control characters ($00-$1F and $7F to $9F) are blank, as is any
character the font hasn't.  The console driver loads it from the SD card's \\OS\\Fonts\\CONSOLE.FNT.

CONSOLE.FNT in this directory was made from Terminus Font 16 (Lat15-Terminus16.psf.gz, as Debian's console-setup has
it), which is under the SIL Open Font License 1.1: Terminus-OFL.txt beside it.

    MakeConsoleFont.py [font.psf.gz] [CONSOLE.FNT]
"""
import gzip
import struct
import sys

GLYPH_WIDTH = 8
GLYPH_HEIGHT = 16
NUM_GLYPHS = 256
DEFAULT_FONT = "/usr/share/consolefonts/Lat15-Terminus16.psf.gz"
DEFAULT_OUTPUT = "CONSOLE.FNT"


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


def IsControl(iCode):
    return (iCode < 0x20) or ((iCode >= 0x7F) and (iCode < 0xA0))


def main(aszArguments):
    szFont = aszArguments[1] if len(aszArguments) > 1 else DEFAULT_FONT
    szOutput = aszArguments[2] if len(aszArguments) > 2 else DEFAULT_OUTPUT
    iWidth, iHeight, aGlyphs, dMap = ReadFont(szFont)
    if (iWidth != GLYPH_WIDTH) or (iHeight != GLYPH_HEIGHT):
        raise ValueError("%s is %dx%d, not %dx%d." % (szFont, iWidth, iHeight, GLYPH_WIDTH, GLYPH_HEIGHT))

    abOutput = bytearray()
    aiMissing = []
    for iCode in range(NUM_GLYPHS):
        if IsControl(iCode):
            abOutput += bytes(GLYPH_HEIGHT)
        elif iCode in dMap:
            abOutput += aGlyphs[dMap[iCode]][:GLYPH_HEIGHT]
        else:
            abOutput += bytes(GLYPH_HEIGHT)
            aiMissing.append(iCode)
    with open(szOutput, "wb") as cFile:
        cFile.write(abOutput)
    print("%s: %d bytes from %s%s" % (szOutput, len(abOutput), szFont, (", blank for " + " ".join("$%02X" % i for i in aiMissing)) if aiMissing else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
