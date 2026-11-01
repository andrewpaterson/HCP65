#include "BaseLib/Chars.h"
#include "BaseLib/Logger.h"
#include "BaseLib/WindowsError.h"
#include <iostream>
#include <string>
#include <cstring>  // for strlen
#include "WindowsCOM.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
HANDLE OpenWindowsCOMPort(char* szCOMName)
{
    CChars          szFullName;
    HANDLE          hSerial;
    char*           szError;
    DCB             dcb;
    COMMTIMEOUTS    timeouts;

    szFullName.Init("\\\\.\\");
    szFullName.Append(szCOMName);

    hSerial = CreateFileA(  szFullName.Text(),
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            OPEN_EXISTING,
                            0,  // Synchronous mode (blocking)
                            NULL);
    szFullName.Kill();

    if (hSerial == INVALID_HANDLE_VALUE)
    {
        szError = WindowsErrorCodeToString(GetLastError());
        gcLogger.Error2(__METHOD__, " Open COM failed: ", szError, NULL);
        return INVALID_HANDLE_VALUE;
    }

    memset(&dcb, 0, sizeof(DCB));
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(hSerial, &dcb)) 
    {
        szError = WindowsErrorCodeToString(GetLastError());
        gcLogger.Error2(__METHOD__, " Open COM failed: ", szError, NULL);
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    if (!EscapeCommFunction(hSerial, SETDTR)) 
    {  // SETDTR raises DTR
    //    std::cerr << "Failed to set DTR: " << GetLastError() << std::endl;
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    // Core settings (8 data bits, no parity, 1 stop bit)
    dcb.BaudRate = CBR_115200;  // Any value works (commonly 115200 or 9600); ignored by Pico
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    // Disable all flow control (Pico doesn't support RTS/CTS or XON/XOFF)
    dcb.fOutxCtsFlow = FALSE;  // No CTS monitoring
    dcb.fOutxDsrFlow = FALSE;  // No DSR monitoring
    dcb.fRtsControl = RTS_CONTROL_ENABLE;  // Or RTS_CONTROL_ENABLE if you want it always high (harmless)
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = FALSE;  // No software output flow control
    dcb.fInX = FALSE;   // No software input flow control
    dcb.fTXContinueOnXoff = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;

    // Binary mode (required for Windows serial ports)
    dcb.fBinary = TRUE;

    // Optional but common
    dcb.fParity = FALSE;  // No parity checking
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(hSerial, &dcb)) 
    {
        szError = WindowsErrorCodeToString(GetLastError());
        gcLogger.Error2(__METHOD__, " Open COM failed: ", szError, NULL);
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    // Set timeouts
    memset(&timeouts, 0, sizeof(COMMTIMEOUTS));

    timeouts.ReadIntervalTimeout = 10;
    timeouts.ReadTotalTimeoutConstant = 10;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 10;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hSerial, &timeouts))
    {
        szError = WindowsErrorCodeToString(GetLastError());
        gcLogger.Error2(__METHOD__, " Open COM failed: ", szError, NULL);
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    return hSerial;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool SendToWindowsCOMPort(HANDLE hSerial, char* szMessage, CChars* pszResult)
{
    char*           szError;
    DWORD           uiBytesWritten;
    char            szResponse[1024];
    DWORD           uiBytesRead;

    // Write data
    if (!WriteFile(hSerial, szMessage, (DWORD)strlen(szMessage), &uiBytesWritten, NULL))
    {
        szError = WindowsErrorCodeToString(GetLastError());
        gcLogger.Error2(__METHOD__, " Write to COM failed: ", szError, NULL);
        CloseHandle(hSerial);
        return false;
    }

    memset(szResponse, 0, 1024);

    if (ReadFile(hSerial, szResponse, 1024 - 1, &uiBytesRead, NULL) && uiBytesRead > 0)
    {
        if (pszResult)
        {
            pszResult->Append(szResponse);
        }
    }

    return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CloseWindowsCOMPort(HANDLE hSerial)
{
    EscapeCommFunction(hSerial, CLRDTR);
    CloseHandle(hSerial);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool IsWindowsCOMPortSuccess(HANDLE hSerial)
{
    return hSerial != INVALID_HANDLE_VALUE;
}

