#ifndef __WINDOWS_COM_H__
#define __WINDOWS_COM_H__
#include <windows.h>
#include "BaseLib/Chars.h"


HANDLE OpenWindowsCOMPort(char* szCOMName);
bool SendToWindowsCOMPort(HANDLE hSerial, char* szMessage, CChars* pszResult = NULL);
void CloseWindowsCOMPort(HANDLE hSerial);
bool IsWindowsCOMPortSuccess(HANDLE hSerial);


#endif // __WINDOWS_COM_H__

