#ifndef __HCP65_MAINBOARD_H__
#define __HCP65_MAINBOARD_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/Chars.h"
#include "HCP65.h"


#define HCP65_MAINBOARD_XML_TAG			"Mainboard"
#define HCP65_MAINBOARD_PROCESSOR_TAG	"Processor"
#define HCP65_MAINBOARD_BOARD_TAG		"Board"
#define HCP65_MAINBOARD_MAX_PROCESSORS	3			//The Full Mainboard's processor sockets.


class CMarkupTag;
class CXMLWriter;


//A board out of its socket.
struct SHCP65SpareBoard
{
	CHCP65Device*	pcBoard;
	const char*		szType;
};


typedef CArrayTemplate<SHCP65SpareBoard>	CArrayHCP65SpareBoards;


//Which mainboard the HCP65's boards are plugged into, which says which device sockets there are.
enum EHCP65Mainboard
{
	HMB_Unknown,
	HMB_Any,		//Board agnostic: one processor with every device socket, 2 to 7 and 16 to 19.
	HMB_Small,		//One processor; 1, 2 and 7 are on the board, leaving sockets 3 and 4 (32KB) and 16 to 19 (128KB).
	HMB_Full,		//Three processors: the first with sockets 3 to 6 and 16 to 19, the others 16 and 17.  Not emulated yet.
};


//The boards plugged into an HCP65's device sockets, from an XML description:
//
//  <Mainboard Type="Small">				Small, Any or Full.
//    <Processor Index="0">					One per processor socket; Index defaults to 0.
//      <Board Socket="3" Type="SD Card Board">
//        <Card Socket="0" File="SDCard.img"/>		Up to four, sockets 0 to 3.
//      </Board>
//      <Board Socket="16" Type="Sound Board"/>
//    </Processor>
//  </Mainboard>
//
//A Board's Type is the name in its device ROM; its other attributes are its own (CHCP65Device::LoadXML), with files
//relative to the description's directory.  A socket must be one the mainboard has, and a board needing a 128KB slot
//one of 16 to 19.  Only one processor is emulated, so only processor 0 can have boards, and the Full Mainboard isn't
//allowed yet.  Socket 1 is the main memory channel the HCP65 always has (CHCP65MemoryChannel).
//
//The mainboard makes and owns the boards: Kill them after the HCP65 they are plugged into.  A subclass can make more
//kinds of board (a test's own) by overriding CreateBoard.
//
//ChangeBoards changes which boards are in the sockets, and is only for the moment of a system reset: reset the HCP65
//straight after it.  A board taken out is kept as it is, with its memory and cards, and is what goes in next when a
//board of its Type is asked for; a new board is only made when there is none.  Write saves the description, without
//the boards that are out, and without --card's image.
class CHCP65Mainboard
{
protected:
	EHCP65Mainboard	meType;
	CHCP65Device*	mapcBoards[HCP65_NUM_DEVICE_SLOTS];		//The emulated processor's sockets, NULL where empty.
	const char*		maszTypes[HCP65_NUM_DEVICE_SLOTS];		//Each board's Type.
	CArrayHCP65SpareBoards	masSpares;						//The boards taken out.
	CChars			mszCard;								//SetCard's, put in the first SD Card Board.

public:
	void			Init(void);
	void			Kill(void);

	void			SetCard(const char* szImageFilename);
	bool			Read(const char* szFilename, CChars* pszError);
	bool			LoadXML(CMarkupTag* pcTag, const char* szDirectory, CChars* pszError);
	bool			Write(const char* szFilename, CChars* pszError);
	void			SaveXML(CXMLWriter* pcWriter);
	bool			PlugIn(CHCP65* pcHCP65);
	bool			ChangeBoards(CHCP65* pcHCP65, const char** aszTypes, CChars* pszError);

	EHCP65Mainboard	GetType(void);
	CHCP65Device*	GetBoard(int iSocket);
	const char*		GetBoardType(int iSocket);
	CHCP65Device*	FindBoard(const char* szType);
	bool			CanHold(int iSocket, const char* szType);

	static int		NumProcessors(EHCP65Mainboard eType);
	static bool		HasSocket(EHCP65Mainboard eType, int iProcessor, int iSocket);
	static uint32	GetSocketSize(int iSocket);
	static size		NumBoardTypes(void);
	static const char*	GetBoardTypeName(size uiIndex);

protected:
	bool			LoadProcessor(CMarkupTag* pcTag, const char* szDirectory, bool* pbSeen, CChars* pszError);
	bool			LoadBoard(CMarkupTag* pcTag, int iProcessor, const char* szDirectory, CChars* pszError);
	bool			PlugInBoard(CHCP65* pcHCP65, int iSocket, const char* szType);
	void			TakeOutBoard(CHCP65* pcHCP65, int iSocket);
	virtual CHCP65Device*	CreateBoard(const char* szType, const char** pszType);
};


bool HCP65XMLError(CChars* pszError, CMarkupTag* pcTag, const char* szMessage, const char* szDetail);
bool HCP65XMLNumber(CMarkupTag* pcTag, const char* szName, bool bRequired, uint32 uiMax, uint32 uiDefault, uint32* puiValue, CChars* pszError);


#endif // __HCP65_MAINBOARD_H__
