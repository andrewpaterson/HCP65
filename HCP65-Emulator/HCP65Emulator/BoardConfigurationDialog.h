#ifndef __BOARD_CONFIGURATION_DIALOG_H__
#define __BOARD_CONFIGURATION_DIALOG_H__
#include "StandardLib/Array.h"
#include "WindowLib/Dialog.h"
#include "HCP65Lib/HCP65Mainboard.h"


#define BOARD_CONFIGURATION_EMPTY	"Empty"		//A socket with no board, in its combo box.


//Which board is in each of the mainboard's device sockets: a combo box for each, of the boards that fit it and Empty,
//starting at the board there now.  It only asks: GetTypes is what was chosen, for CHCP65Mainboard::ChangeBoards.
//
//A dialog is an object: make it with OMalloc and keep it in the root, or an object that is, while it's shown.
class CBoardConfigurationDialog : public CObject
{
CONSTRUCTABLE(CBoardConfigurationDialog);
DESTRUCTABLE(CBoardConfigurationDialog);
protected:
	Ptr<CDialog>		mpDialog;
	CArray<CComboBox>	maCombos;		//One for each of the mainboard's sockets, lowest first.
	CHCP65Mainboard*	mpcMainboard;

public:
	void	Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CHCP65Mainboard* pcMainboard);
	void	Class(void) override;
	void	Free(void) override;

	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool	Show(void);
	void	GetTypes(const char** aszTypes);

protected:
	void	AddSocket(int iSocket);
};


#endif // __BOARD_CONFIGURATION_DIALOG_H__
