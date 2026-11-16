#ifndef __HCP65_TERMINAL_H__
#define __HCP65_TERMINAL_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayUint8.h"
#include "BaseLib/Mutex.h"
#include "HCP65UART.h"


#define HCP65_TERMINAL_MAX_LINES	500		//The oldest go.
#define HCP65_TERMINAL_TAB			8


//A terminal at the far end of a UART's line (Attach): it shows what the HCP65 sends, as a teletype does.  Attached, the
//far end's RTS and DTR are on (the UART's CTS, DSR and DCD).
//
//Received is on the HCP65's thread: the characters are queued under a lock.  Update, on the thread that shows them,
//applies them to the lines: a printable character is put at the column (over what is there) and moves it on; CR goes
//back to the start of the line, LF to a new line, backspace back one, tab to the next multiple of eight and form feed
//clears it all.  Anything else is ignored.  Only the last HCP65_TERMINAL_MAX_LINES lines are kept.
class CHCP65Terminal : public CHCP65UARTHost
{
protected:
	CHCP65UART*		mpcUART;
	CMutex			mcMutex;
	CArrayUint8		macPending;		//Under mcMutex.
	CArrayChars		maszLines;		//Never empty: the last is the one being written.
	size			muiColumn;

public:
	void		Init(void);
	void		Kill(void);

	void		Attach(CHCP65UART* pcUART);
	void		Detach(void);
	CHCP65UART*	GetUART(void);

	void		Received(CHCP65UART* pcUART, uint8 uiCharacter) override;
	bool		Update(void);

	void		Put(uint8 uiCharacter);
	void		Clear(void);
	size		NumLines(void);
	char*		GetLine(size uiLine);
	size		GetColumn(void);
	void		GetText(CChars* pszDest);
};


#endif // __HCP65_TERMINAL_H__

