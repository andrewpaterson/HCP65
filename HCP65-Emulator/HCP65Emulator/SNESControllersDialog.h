#ifndef __SNES_CONTROLLERS_DIALOG_H__
#define __SNES_CONTROLLERS_DIALOG_H__
#include "StandardLib/Array.h"
#include "WindowLib/ComboBox.h"
#include "WindowLib/Dialog.h"
#include "HCP65SNESKeys.h"


//Which of the host's keys drive each of the LCD and SNES Board's two SNES controllers: a combo box for each, of the key
//mappings (CHCP65SNESKeys), starting at the controller's mapping now.  It only asks: GetMapping is what was chosen.
//Without the board it says so, and has nothing to choose.
//
//A dialog is an object: make it with OMalloc and keep it in the root, or an object that is, while it's shown.
class CSNESControllersDialog : public CObject
{
CONSTRUCTABLE(CSNESControllersDialog);
DESTRUCTABLE(CSNESControllersDialog);
protected:
	Ptr<CDialog>			mpDialog;
	CArray<CComboBox>		maCombos;		//One for each controller, while there is a board.

public:
	void		Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CHCP65SNESKeys* pcKeys);
	void		Class(void) override;
	void		Free(void) override;

	bool		Save(CObjectWriter* pcFile) override;
	bool		Load(CObjectReader* pcFile) override;

	bool		Show(void);
	bool		HasControllers(void);
	size		GetMapping(int iController);

protected:
	void		AddController(int iController, size uiMapping);
};


#endif // __SNES_CONTROLLERS_DIALOG_H__
