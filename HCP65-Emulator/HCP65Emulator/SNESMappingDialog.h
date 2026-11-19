#ifndef __SNES_MAPPING_DIALOG_H__
#define __SNES_MAPPING_DIALOG_H__
#include "StandardLib/Array.h"
#include "WindowLib/Canvas.h"
#include "WindowLib/ComboBox.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/TextField.h"
#include "HCP65SNESKeys.h"
#include "SNESControllerDraw.h"
#include "SNESKeyField.h"


#define SNES_MAPPING_NEW			"New mapping"		//The mapping combo box's last item.
#define SNES_MAPPING_FRAME_MS		30					//Between looks at the keys held.
#define SNES_MAPPING_COLUMNS		4					//Buttons in each row of key fields.


//Makes or changes one of the emulator's SNES key mappings (CHCP65SNESKeys): which mapping (an existing one, or a new
//one), its name, and on top a key field (CSNESKeyField) for each of the controller's buttons, over a picture of the
//controller (CSNESControllerDraw).  Choosing a mapping fills in its name and keys.  The picture shows the button whose
//field has the focus, and lights up each button whose key is held, so a mapping can be tried before it is kept.  It
//only asks: GetMappingName and GetKey are what was chosen, for CHCP65SNESKeys::SetMapping.
//
//A dialog is an object: make it with OMalloc and keep it in the root, or an object that is, while it's shown.
class CSNESMappingDialog : public CObject
{
CONSTRUCTABLE(CSNESMappingDialog);
DESTRUCTABLE(CSNESMappingDialog);
protected:
	Ptr<CDialog>				mpDialog;
	Ptr<CComboBox>				mpMappings;
	Ptr<CTextField>				mpName;
	CArray<CSNESKeyField>		maFields;		//One for each button, in the order a controller sends them.
	Ptr<CCanvas>				mpCanvas;
	Ptr<CSNESControllerDraw>	mpController;
	CHCP65SNESKeys*				mpcKeys;

public:
	void		Init(Ptr<CWindow> pOwner, SDialogLook* psLook, Ptr<CWindowTheme> pTheme, CHCP65SNESKeys* pcKeys, size uiMapping);
	void		Class(void) override;
	void		Free(void) override;

	bool		Save(CObjectWriter* pcFile) override;
	bool		Load(CObjectReader* pcFile) override;

	bool		Show(void);
	const char*	GetMappingName(void);
	const char*	GetKey(int iButton);

	void		Tick(void);

protected:
	void		MappingChosen(void);
	void		Fill(size uiMapping);
	Ptr<CGridContainer>	MakeFields(Ptr<CWindowTheme> pTheme);
};


#endif // __SNES_MAPPING_DIALOG_H__
