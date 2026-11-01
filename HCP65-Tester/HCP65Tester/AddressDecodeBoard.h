#ifndef __ADDRESS_DECODE_BOARD_H__
#define __ADDRESS_DECODE_BOARD_H__
#include "CommonBoard.h"


void SetupAddressDecode(CBoardPins* pcBoard);
bool TestAddressDecode(CUART* pcUART, CBoardPins* pcBoard);


#endif // __ADDRESS_DECODE_BOARD_H__

