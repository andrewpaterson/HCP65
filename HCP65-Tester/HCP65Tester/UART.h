#ifndef __UART_H__
#define __UART_H__
#include "BaseLib/Chars.h"
#include "WindowsCOM.h"


class CUART
{
protected:
	HANDLE	mhSerial;
	CChars	mszCOMName;

public:
	void Init(char* szCOMName);
	void Kill(void);

	bool Open(void);
	bool Close(void);

	bool Send(char* szMessage, CChars* pszResult = NULL);
};


#endif // __UART_H__

