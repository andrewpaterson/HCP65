#ifndef __COMMON_BOARD_H__
#define __COMMON_BOARD_H__
#include "BaseLib/DebugOutput.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/LogString.h"
#include "BoardPins.h"
#include "UART.h"


void SetupStandard56PinBoard(CBoardPins* pcBoard, char* szName, char* szVersion);


#endif // __COMMON_BOARD_H__

