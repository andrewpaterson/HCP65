#ifndef __BOARD_CONFIGURATION_DIALOG_H__
#define __BOARD_CONFIGURATION_DIALOG_H__
#include "StandardLib/Array.h"
#include "WindowLib/Canvas.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/GridContainer.h"
#include "HCP65Lib/HCP65Mainboard.h"
#include "BoardScene.h"


#define BOARD_CONFIGURATION_EMPTY		"Empty"		//A socket with no board, in its combo box.
#define BOARD_CONFIGURATION_FRAME_MS	50			//Between one picture and the next.
#define BOARD_CONFIGURATION_TURN_MS		30000		//How long the mainboard takes to go right round.
#define BOARD_CONFIGURATION_SIDE_SPACE	160			//Each side of the picture and the rows when the dialog opens.


//Which board is in each of the mainboard's device sockets: a combo box for each, of the boards that fit it and Empty,
//starting at the board there now, under a turning picture of the mainboard with what's chosen in its sockets, if it has
//one.  It only asks: GetTypes is what was chosen, for CHCP65Mainboard::ChangeBoards.
//
//The picture's models are loaded while the dialog is up, one at a tick, as each can take seconds the first time (see
//CBoardSceneDraw), during which the dialog doesn't answer.
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
	CHCP65Mainboard*		mpcMainboard;
	Ptr<CCanvas>			mpSceneCanvas;
	Ptr<CBoardSceneDraw>	mpScene;		//NULL for no picture.
	bool					mbPainted;		//The dialog has been painted, so a model can be loaded.
	int64					miStartTime;	//When the mainboard started turning, -1 before it has.

public:
	void		Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CHCP65Mainboard* pcMainboard, Ptr<CBoardSceneDraw> pScene);
	void		Class(void) override;
	void		Free(void) override;

	bool		Save(CObjectWriter* pcFile) override;
	bool		Load(CObjectReader* pcFile) override;

	bool		Show(void);
	void		GetTypes(const char** aszTypes);

	void		TickScene(void);

protected:
	void		AddSocket(int iSocket);
	void		AddSideSpace(Ptr<CGridContainer> pGrid, int iColumn);
	void		AddRowGap(void);
};


#endif // __BOARD_CONFIGURATION_DIALOG_H__
