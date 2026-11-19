#ifndef __BOARD_CONFIGURATION_DIALOG_H__
#define __BOARD_CONFIGURATION_DIALOG_H__
#include "StandardLib/Array.h"
#include "BaseLib/ArrayInt.h"
#include "WindowLib/Canvas.h"
#include "WindowLib/CheckBox.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/GridContainer.h"
#include "HCP65Lib/HCP65Mainboard.h"
#include "HCP65Lib/HCP65VIA.h"
#include "BoardScene.h"


#define BOARD_CONFIGURATION_EMPTY		"Empty"		//A socket with no board, in its combo box.
#define BOARD_CONFIGURATION_FRAME_MS	50			//Between one picture and the next.
#define BOARD_CONFIGURATION_TURN_MS		30000		//How long the mainboard takes to go right round.
#define BOARD_CONFIGURATION_SIDE_SPACE	160			//Each side of the picture and the rows when the dialog opens.
#define BOARD_CONFIGURATION_ALL_BREAKOUTS	0x0F	//GetBreakouts: every VIA Breakout fitted.
#define BOARD_CONFIGURATION_COLUMNS		3		//The rows' label, combo box and VIA Breakouts: the picture spans them all.


//Which board is in each of the mainboard's device sockets: a combo box for each, of the boards that fit it and Empty,
//starting at the board there now, under a turning picture of the mainboard with what's chosen in its sockets, if it has
//one.  It only asks: GetTypes is what was chosen, for CHCP65Mainboard::ChangeBoards.
//
//While a socket's combo box has the VIA Board, a check box for each of its VIA Breakouts is beside it (AB/CD is breakout
//0), starting at the breakouts of the VIA Board in the socket now, or all four.  At least one stays ticked: unticking
//the last ticks it again.  GetBreakouts is what was ticked.  They go beside the combo box, not under it, as the dialog's
//height is set when it is shown.
//
//The picture's models are read while the dialog is up, on threads of the scene's (CBoardSceneDraw), as each can take
//seconds the first time: a board appears in its socket when its model has been read, and the dialog answers meanwhile.
//
//The picture and the rows are in a column of BOARD_SCENE_WIDTH in the middle of the dialog, with what's left over shared out
//either side, so they keep their size however wide the dialog is.
//
//A dialog is an object: make it with OMalloc and keep it in the root, or an object that is, while it's shown.
class CBoardConfigurationDialog : public CObject
{
CONSTRUCTABLE(CBoardConfigurationDialog);
DESTRUCTABLE(CBoardConfigurationDialog);
protected:
	Ptr<CDialog>			mpDialog;
	Ptr<CGridContainer>		mpRows;			//The middle column.
	int32					miNumRows;
	CArray<CComboBox>		maCombos;		//One for each of the mainboard's sockets, in the order of the rows.
	CArrayInt				maiComboSockets;	//Each combo box's socket.
	CArrayInt				maiComboRows;		//Each combo box's row of mpRows.
	CArray<CGridContainer>	maBreakouts;		//Each combo box's breakouts' check boxes, in column 2 of its row while it has the VIA Board.
	CArrayInt				maiBreakoutsShown;	//Each combo box's breakouts are in mpRows.
	CArray<CCheckBox>		maBreakoutBoxes;	//HCP65_VIA_NUM_BREAKOUTS for each combo box.
	CHCP65Mainboard*		mpcMainboard;
	Ptr<CCanvas>			mpSceneCanvas;
	Ptr<CBoardSceneDraw>	mpScene;		//NULL for no picture.
	int64					miStartTime;	//When the mainboard started turning, -1 before it has.

public:
	void		Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CHCP65Mainboard* pcMainboard, Ptr<CBoardSceneDraw> pScene);
	void		Class(void) override;
	void		Free(void) override;

	bool		Save(CObjectWriter* pcFile) override;
	bool		Load(CObjectReader* pcFile) override;

	bool		Show(void);
	void		GetTypes(const char** aszTypes);
	uint8		GetBreakouts(int iSocket);

	void		TickScene(void);

protected:
	void		AddSocket(int iSocket);
	void		AddSideSpace(Ptr<CGridContainer> pGrid, int iColumn);
	void		AddRowGap(void);
	void		AddBreakouts(int iSocket);
	void		ShowBreakouts(void);
	void		BreakoutChanged(CCheckBox* pcBox);
};


#endif // __BOARD_CONFIGURATION_DIALOG_H__
