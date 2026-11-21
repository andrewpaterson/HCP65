#include "BaseLib/FNVHash.h"
#include "HCP65SFT.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" bool SFTIsRecord(const void* pv, uint32 uiSize, uint8 uiType)
{
	const SSFTHeader*	psHeader;

	psHeader = (const SSFTHeader*)pv;
	return (uiSize >= sizeof(SSFTHeader)) && (psHeader->uiMagic == SFT_MAGIC) && (psHeader->uiVersion == SFT_VERSION) && (psHeader->uiType == uiType);
}


//////////////////////////////////////////////////////////////////////////
//
// The container's uiIndex'th image (palettes not counted) and its size,
// and the last palette before it in *ppsPalette (NULL if none).  NULL if
// there isn't one.
//
//////////////////////////////////////////////////////////////////////////
static const uint8* SFTContainedRecord(const void* pvContainer, uint32 uiSize, uint16 uiIndex, uint32* puiSize, const SSFTPalette** ppsPalette)
{
	const uint8*			puiContainer;
	const uint8*			puiRecord;
	const SSFTContainer*	psContainer;
	uint32					uiPosition;
	uint32					uiRecordSize;
	uint16					uiNext;
	uint16					uiImage;
	uint16					i;

	puiContainer = (const uint8*)pvContainer;
	psContainer = (const SSFTContainer*)pvContainer;
	*ppsPalette = NULL;
	if (!SFTIsRecord(pvContainer, uiSize, SFT_TYPE_CONTAINER) || (uiSize < sizeof(SSFTContainer)))
	{
		return NULL;
	}

	uiPosition = sizeof(SSFTContainer);
	uiImage = 0;
	for (i = 0; i < psContainer->uiCount; i++)
	{
		if (uiPosition + sizeof(uint16) > uiSize)
		{
			return NULL;
		}
		uiNext = *((const uint16*)&puiContainer[uiPosition]);
		puiRecord = &puiContainer[uiPosition + sizeof(uint16)];
		uiRecordSize = ((uiNext != 0) && (uiPosition + uiNext <= uiSize)) ? (uint32)(uiNext - sizeof(uint16)) : (uiSize - uiPosition - sizeof(uint16));
		if (SFTIsRecord(puiRecord, uiRecordSize, SFT_TYPE_PALETTE))
		{
			*ppsPalette = (const SSFTPalette*)puiRecord;
		}
		else
		{
			if (uiImage == uiIndex)
			{
				*puiSize = uiRecordSize;
				return puiRecord;
			}
			uiImage++;
		}
		if (uiNext == 0)
		{
			return NULL;
		}
		uiPosition += uiNext;
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" const void* SFTContainedImage(const void* pvContainer, uint32 uiSize, uint16 uiIndex, uint32* puiSize)
{
	const SSFTPalette*	psPalette;

	return SFTContainedRecord(pvContainer, uiSize, uiIndex, puiSize, &psPalette);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" const SSFTPalette* SFTContainedPalette(const void* pvContainer, uint32 uiSize, uint16 uiIndex)
{
	const SSFTPalette*	psPalette;
	uint32				uiRecordSize;

	if (!SFTContainedRecord(pvContainer, uiSize, uiIndex, &uiRecordSize, &psPalette))
	{
		return NULL;
	}
	return psPalette;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint32 SFTPaletteHash(const SSFTPalette* psPalette)
{
	const uint8*	puiColours;
	uint8			auiNumbers[4];
	uint32			uiHash;

	auiNumbers[0] = (uint8)psPalette->uiFirst;
	auiNumbers[1] = (uint8)(psPalette->uiFirst >> 8);
	auiNumbers[2] = (uint8)psPalette->uiCount;
	auiNumbers[3] = (uint8)(psPalette->uiCount >> 8);
	puiColours = (const uint8*)psPalette + sizeof(SSFTPalette);
	uiHash = FNVHash32(auiNumbers, 4);
	return FNVHash32(puiColours, (lsize)psPalette->uiCount * 3, uiHash);
}


//////////////////////////////////////////////////////////////////////////
//
// Three bits and two bits repeated to fill eight, as SupportLib's
// SFTReader makes them.
//
//////////////////////////////////////////////////////////////////////////
extern "C" void SFTColours(void* pvPalette)
{
	SSFTPalette*	psPalette;
	uint8*			puiRed;
	uint8*			puiGreen;
	uint8*			puiBlue;
	uint16			i;
	uint8			uiRed;
	uint8			uiGreen;
	uint8			uiBlue;

	psPalette = (SSFTPalette*)pvPalette;
	psPalette->uiMagic = SFT_MAGIC;
	psPalette->uiVersion = SFT_VERSION;
	psPalette->uiType = SFT_TYPE_PALETTE;
	psPalette->uiFirst = 0;
	psPalette->uiCount = SFT_PALETTE_COLOURS;
	psPalette->uiHash = SFT_COLOURS_HASH;
	puiRed = (uint8*)pvPalette + sizeof(SSFTPalette);
	puiGreen = puiRed + SFT_PALETTE_COLOURS;
	puiBlue = puiGreen + SFT_PALETTE_COLOURS;
	for (i = 0; i < SFT_PALETTE_COLOURS; i++)
	{
		uiRed = (uint8)(i & 7);
		uiGreen = (uint8)((i >> 3) & 7);
		uiBlue = (uint8)((i >> 6) & 3);
		puiRed[i] = (uint8)((uiRed << 5) | (uiRed << 2) | (uiRed >> 1));
		puiGreen[i] = (uint8)((uiGreen << 5) | (uiGreen << 2) | (uiGreen >> 1));
		puiBlue[i] = (uint8)((uiBlue << 6) | (uiBlue << 4) | (uiBlue << 2) | uiBlue);
	}
}
