#ifndef __SNES_KEY_FIELD_H__
#define __SNES_KEY_FIELD_H__
#include "WindowLib/TextField.h"


//A field that is the one key pressed in it, named as the input names it (as the PS/2 keyboard and CHCP65SNESKeys take
//them): any key, Shift, Control, Enter and Tab too, as a controller's button can be any of them.  It has it before it can
//mean anything else.  Backspace empties it (no key), and Escape goes on, to the dialog's Cancel.
class CSNESKeyField : public CTextField
{
CONSTRUCTABLE(CSNESKeyField);
DESTRUCTABLE(CSNESKeyField);
public:
	void	Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, const char* szKey);
	void	Class(void) override;
	void	Free(void) override;

	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	KeyDownAction(SComponentInputEvent* psEvent);
};


#endif // __SNES_KEY_FIELD_H__
