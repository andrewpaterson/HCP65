#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/ConstructorCall.h"
#include "BaseLib/PointerFunctions.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/XMLFile.h"
#include "BaseLib/MarkupTag.h"
#include "HCP65Audio.h"
#include "HCP65SDCard.h"
#include "HCP65UART.h"
#include "HCP65MemoryBoard.h"
#include "HCP65Mainboard.h"


//The mainboards' Type attributes, in EHCP65Mainboard order from HMB_Any.
static const char*	gaszHCP65MainboardTypes[] = { "Any", "Small", "Full" };


//////////////////////////////////////////////////////////////////////////
//
// pszError is szMessage and szDetail, then pcTag and its line.  Always
// false.
//
//////////////////////////////////////////////////////////////////////////
bool HCP65XMLError(CChars* pszError, CMarkupTag* pcTag, const char* szMessage, const char* szDetail)
{
	char	szLine[96];

	pszError->Set(szMessage);
	if (szDetail)
	{
		pszError->Append(szDetail);
	}
	snprintf(szLine, sizeof(szLine), " (<%s> line %lld)", pcTag->GetName(), (long long)pcTag->GetLine() + 1);
	pszError->Append(szLine);
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The number in pcTag's szName attribute (decimal or 0x hexadecimal), up
// to uiMax; uiDefault if the attribute isn't there and isn't required.
//
//////////////////////////////////////////////////////////////////////////
bool HCP65XMLNumber(CMarkupTag* pcTag, const char* szName, bool bRequired, uint32 uiMax, uint32 uiDefault, uint32* puiValue, CChars* pszError)
{
	char*		szValue;
	char*		szEnd;
	long long	lli;

	szValue = pcTag->GetStringAttribute(szName);
	if (szValue == NULL)
	{
		if (bRequired)
		{
			return HCP65XMLError(pszError, pcTag, "Missing ", szName);
		}
		*puiValue = uiDefault;
		return true;
	}
	lli = strtoll(szValue, &szEnd, 0);
	if ((*szValue == '\0') || (*szEnd != '\0') || (lli < 0) || (lli > (long long)uiMax))
	{
		return HCP65XMLError(pszError, pcTag, "Bad number in ", szName);
	}
	*puiValue = (uint32)lli;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// No boards, and no mainboard until one is read.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Mainboard::Init(void)
{
	int		i;

	meType = HMB_Unknown;
	for (i = 0; i < HCP65_NUM_DEVICE_SLOTS; i++)
	{
		mapcBoards[i] = NULL;
		maszTypes[i] = NULL;
	}
	mszCard.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// Kills and frees the boards: the HCP65 they were plugged into must be
// killed first.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Mainboard::Kill(void)
{
	int		i;

	for (i = 0; i < HCP65_NUM_DEVICE_SLOTS; i++)
	{
		SafeKill(mapcBoards[i]);
		maszTypes[i] = NULL;
	}
	mszCard.Kill();
	meType = HMB_Unknown;
}


//////////////////////////////////////////////////////////////////////////
//
// The disk image the first SD Card Board gets in card socket 0 in place
// of its socket 0 <Card> (the emulator's --card).  Call it before reading.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Mainboard::SetCard(const char* szImageFilename)
{
	mszCard.Set(szImageFilename);
}


//////////////////////////////////////////////////////////////////////////
//
// The boards described by the XML file szFilename (see HCP65Mainboard.h).
// False, with pszError saying why, if it can't be read or is wrong.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65Mainboard::Read(const char* szFilename, CChars* pszError)
{
	CXMLFile	cFile;
	CFileUtil	cFileUtil;
	CChars		szName;
	CChars		szDirectory;
	CChars		szError;
	CMarkupTag*	pcRoot;
	bool		bResult;

	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFilename, &szName, &szDirectory);
	if (szDirectory.Empty())
	{
		szDirectory.Set(".");
	}

	cFile.Init();
	pcRoot = NULL;
	if (cFileUtil.Exists(szFilename) && cFile.Read(szName.Text(), szDirectory.Text()) && cFile.mcMarkup.mpcDoc)
	{
		pcRoot = cFile.mcMarkup.mpcDoc->GetRootTag();
	}
	if (!pcRoot)
	{
		pszError->Set("Could not read the mainboard description ");
		pszError->Append(szFilename);
		bResult = false;
	}
	else
	{
		szError.Init();
		bResult = LoadXML(pcRoot, szDirectory.Text(), &szError);
		if (!bResult)
		{
			pszError->Set(szFilename);
			pszError->Append(": ");
			pszError->Append(szError.Text());
		}
		szError.Kill();
	}

	cFile.Kill();
	szDirectory.Kill();
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The boards from a <Mainboard> tag, whose files are relative to
// szDirectory.  False, with pszError saying why, if it's wrong.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65Mainboard::LoadXML(CMarkupTag* pcTag, const char* szDirectory, CChars* pszError)
{
	char*			szType;
	CMarkupTag*		pcProcessor;
	STagIterator	sIterator;
	bool			abSeen[HCP65_MAINBOARD_MAX_PROCESSORS];
	int				i;

	if (strcmp(pcTag->GetName(), HCP65_MAINBOARD_XML_TAG) != 0)
	{
		return HCP65XMLError(pszError, pcTag, "Expected <" HCP65_MAINBOARD_XML_TAG ">", NULL);
	}
	szType = pcTag->GetStringAttribute("Type");
	if (szType == NULL)
	{
		return HCP65XMLError(pszError, pcTag, "Missing Type", NULL);
	}
	meType = HMB_Unknown;
	for (i = 0; i < (int)(sizeof(gaszHCP65MainboardTypes) / sizeof(gaszHCP65MainboardTypes[0])); i++)
	{
		if (strcmp(szType, gaszHCP65MainboardTypes[i]) == 0)
		{
			meType = (EHCP65Mainboard)(HMB_Any + i);
		}
	}
	if (meType == HMB_Unknown)
	{
		return HCP65XMLError(pszError, pcTag, "Type must be Any, Small or Full, not ", szType);
	}
	if (meType == HMB_Full)
	{
		return HCP65XMLError(pszError, pcTag, "The Full Mainboard isn't emulated yet", NULL);
	}

	for (i = 0; i < HCP65_MAINBOARD_MAX_PROCESSORS; i++)
	{
		abSeen[i] = false;
	}
	for (pcProcessor = pcTag->GetTag(&sIterator); pcProcessor; pcProcessor = pcTag->GetNextTag(&sIterator))
	{
		if (!LoadProcessor(pcProcessor, szDirectory, abSeen, pszError))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A <Processor> tag's boards.  pbSeen says which processors have had
// theirs.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65Mainboard::LoadProcessor(CMarkupTag* pcTag, const char* szDirectory, bool* pbSeen, CChars* pszError)
{
	uint32			uiIndex;
	CMarkupTag*		pcBoard;
	STagIterator	sIterator;

	if (strcmp(pcTag->GetName(), HCP65_MAINBOARD_PROCESSOR_TAG) != 0)
	{
		return HCP65XMLError(pszError, pcTag, "Expected <" HCP65_MAINBOARD_PROCESSOR_TAG ">", NULL);
	}
	if (!HCP65XMLNumber(pcTag, "Index", false, HCP65_MAINBOARD_MAX_PROCESSORS - 1, 0, &uiIndex, pszError))
	{
		return false;
	}
	if ((int)uiIndex >= NumProcessors(meType))
	{
		return HCP65XMLError(pszError, pcTag, "This mainboard has no such processor socket", NULL);
	}
	if (pbSeen[uiIndex])
	{
		return HCP65XMLError(pszError, pcTag, "The processor is described twice", NULL);
	}
	pbSeen[uiIndex] = true;

	for (pcBoard = pcTag->GetTag(&sIterator); pcBoard; pcBoard = pcTag->GetNextTag(&sIterator))
	{
		if (!LoadBoard(pcBoard, uiIndex, szDirectory, pszError))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A <Board> tag: the board is made, set up from the tag, and kept for its
// socket.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65Mainboard::LoadBoard(CMarkupTag* pcTag, int iProcessor, const char* szDirectory, CChars* pszError)
{
	uint32			uiSocket;
	char*			szType;
	const char*		szKnownType;
	CHCP65Device*	pcBoard;

	if (strcmp(pcTag->GetName(), HCP65_MAINBOARD_BOARD_TAG) != 0)
	{
		return HCP65XMLError(pszError, pcTag, "Expected <" HCP65_MAINBOARD_BOARD_TAG ">", NULL);
	}
	if (!HCP65XMLNumber(pcTag, "Socket", true, HCP65_NUM_DEVICE_SLOTS - 1, 0, &uiSocket, pszError))
	{
		return false;
	}
	if (!HasSocket(meType, iProcessor, uiSocket))
	{
		return HCP65XMLError(pszError, pcTag, "This mainboard has no such device socket", NULL);
	}
	if (mapcBoards[uiSocket])
	{
		return HCP65XMLError(pszError, pcTag, "A board is in this socket already: ", maszTypes[uiSocket]);
	}
	szType = pcTag->GetStringAttribute("Type");
	if (szType == NULL)
	{
		return HCP65XMLError(pszError, pcTag, "Missing Type", NULL);
	}
	pcBoard = CreateBoard(szType, &szKnownType);
	if (pcBoard == NULL)
	{
		return HCP65XMLError(pszError, pcTag, "Unknown board Type: ", szType);
	}
	mapcBoards[uiSocket] = pcBoard;
	maszTypes[uiSocket] = szKnownType;

	if (pcBoard->GetMinimumSlotSize() > GetSocketSize(uiSocket))
	{
		return HCP65XMLError(pszError, pcTag, "The board needs a 128KB socket (16 to 19): ", szType);
	}

	//--card's image goes in the first SD Card Board, in place of its Card.
	if (!mszCard.Empty() && (strcmp(szKnownType, HCP65_SD_NAME) == 0) && (FindBoard(HCP65_SD_NAME) == pcBoard))
	{
		if (!((CHCP65SDCard*)pcBoard)->InsertCard(mszCard.Text(), false))
		{
			return HCP65XMLError(pszError, pcTag, "Could not insert the SD card ", mszCard.Text());
		}
	}
	return pcBoard->LoadXML(pcTag, szDirectory, pszError);
}


//////////////////////////////////////////////////////////////////////////
//
// A new board whose device ROM names it szType (its Init'd), and the type's
// own name in pszType (a string that lasts as long as the mainboard); NULL
// if there's no such board.
//
//////////////////////////////////////////////////////////////////////////
CHCP65Device* CHCP65Mainboard::CreateBoard(const char* szType, const char** pszType)
{
	CHCP65Audio*		pcAudio;
	CHCP65SDCard*		pcSDCard;
	CHCP65UARTBoard*	pcUART;
	CHCP65MemoryBoard*	pcMemory;

	if (strcmp(szType, HCP65_AUDIO_NAME) == 0)
	{
		pcAudio = NewMalloc<CHCP65Audio>();
		pcAudio->Init();
		*pszType = HCP65_AUDIO_NAME;
		return pcAudio;
	}
	if (strcmp(szType, HCP65_SD_NAME) == 0)
	{
		pcSDCard = NewMalloc<CHCP65SDCard>();
		pcSDCard->Init();
		*pszType = HCP65_SD_NAME;
		return pcSDCard;
	}
	if (strcmp(szType, HCP65_UART_NAME) == 0)
	{
		pcUART = NewMalloc<CHCP65UARTBoard>();
		pcUART->Init();
		*pszType = HCP65_UART_NAME;
		return pcUART;
	}
	if (strcmp(szType, HCP65_MEMORY_BOARD_NAME) == 0)
	{
		pcMemory = NewMalloc<CHCP65MemoryBoard>();
		pcMemory->Init();
		*pszType = HCP65_MEMORY_BOARD_NAME;
		return pcMemory;
	}
	*pszType = NULL;
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Puts each board in its socket of pcHCP65.  False if one wouldn't go in
// (LoadXML has checked they all will).
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65Mainboard::PlugIn(CHCP65* pcHCP65)
{
	int		i;
	bool	bResult;

	bResult = true;
	for (i = 0; i < HCP65_NUM_DEVICE_SLOTS; i++)
	{
		if (mapcBoards[i])
		{
			bResult = pcHCP65->SetDevice(i, mapcBoards[i]) && bResult;
		}
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
EHCP65Mainboard CHCP65Mainboard::GetType(void)
{
	return meType;
}


//////////////////////////////////////////////////////////////////////////
//
// The board in the emulated processor's iSocket, or NULL.
//
//////////////////////////////////////////////////////////////////////////
CHCP65Device* CHCP65Mainboard::GetBoard(int iSocket)
{
	if ((iSocket < 0) || (iSocket >= HCP65_NUM_DEVICE_SLOTS))
	{
		return NULL;
	}
	return mapcBoards[iSocket];
}


//////////////////////////////////////////////////////////////////////////
//
// The Type of the board in iSocket (the name in its device ROM), or NULL.
//
//////////////////////////////////////////////////////////////////////////
const char* CHCP65Mainboard::GetBoardType(int iSocket)
{
	if ((iSocket < 0) || (iSocket >= HCP65_NUM_DEVICE_SLOTS))
	{
		return NULL;
	}
	return maszTypes[iSocket];
}


//////////////////////////////////////////////////////////////////////////
//
// The board of szType in the lowest socket, or NULL if there's none.  It
// is the class CreateBoard makes for szType.
//
//////////////////////////////////////////////////////////////////////////
CHCP65Device* CHCP65Mainboard::FindBoard(const char* szType)
{
	int		i;

	for (i = 0; i < HCP65_NUM_DEVICE_SLOTS; i++)
	{
		if (maszTypes[i] && (strcmp(maszTypes[i], szType) == 0))
		{
			return mapcBoards[i];
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether processor iProcessor of an eType mainboard has device socket
// iSocket for a board.  0 is the processor board's registers and 1 the
// main memory channel on every mainboard; the Small Mainboard has its
// status and clock devices in 2 and its processor reset device in 7, and
// nothing connects 5 and 6.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65Mainboard::HasSocket(EHCP65Mainboard eType, int iProcessor, int iSocket)
{
	switch (eType)
	{
	case HMB_Any:
		return (iProcessor == 0) && (((iSocket >= 2) && (iSocket <= 7)) || ((iSocket >= 16) && (iSocket <= 19)));
	case HMB_Small:
		return (iProcessor == 0) && ((iSocket == 3) || (iSocket == 4) || ((iSocket >= 16) && (iSocket <= 19)));
	case HMB_Full:
		if (iProcessor == 0)
		{
			return ((iSocket >= 3) && (iSocket <= 6)) || ((iSocket >= 16) && (iSocket <= 19));
		}
		return ((iProcessor == 1) || (iProcessor == 2)) && ((iSocket == 16) || (iSocket == 17));
	default:
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// How many processor sockets an eType mainboard has.
//
//////////////////////////////////////////////////////////////////////////
int CHCP65Mainboard::NumProcessors(EHCP65Mainboard eType)
{
	switch (eType)
	{
	case HMB_Any:
	case HMB_Small:
		return 1;
	case HMB_Full:
		return HCP65_MAINBOARD_MAX_PROCESSORS;
	default:
		return 0;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The size of device socket iSocket's slot.
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65Mainboard::GetSocketSize(int iSocket)
{
	return (iSocket >= 16) ? HCP65_LARGE_SLOT_SIZE : HCP65_SMALL_SLOT_SIZE;
}
