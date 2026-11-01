#include "CommonBoard.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SetupStandard24PinBoard(CBoardPins* pcBoard, char* szName, char* szVersion)
{
	pcBoard->SetName(szName);
	pcBoard->SetVersion(szVersion);

	pcBoard->AddPower(1, PP_5V);
	pcBoard->AddPower(4, PP_Ground);
	pcBoard->AddPower(9, PP_Ground);

	pcBoard->AddPower(21, PP_Ground);
	pcBoard->AddPower(16, PP_Ground);
	pcBoard->AddPower(13, PP_5V);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SetupAlternate24PinBoard(CBoardPins* pcBoard, char* szName, char* szVersion)
{
	pcBoard->SetName(szName);
	pcBoard->SetVersion(szVersion);

	pcBoard->AddPower(12, PP_5V);

	pcBoard->AddPower(24, PP_Ground);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SetupStandard32PinBoard(CBoardPins* pcBoard, char* szName, char* szVersion)
{
	pcBoard->SetName(szName);
	pcBoard->SetVersion(szVersion);

	pcBoard->AddPower(1, PP_5V);
	pcBoard->AddPower(5, PP_Ground);
	pcBoard->AddPower(12, PP_Ground);

	pcBoard->AddPower(28, PP_Ground);
	pcBoard->AddPower(21, PP_Ground);
	pcBoard->AddPower(17, PP_5V);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SetupAlternate32PinBoard(CBoardPins* pcBoard, char* szName, char* szVersion)
{
	pcBoard->SetName(szName);
	pcBoard->SetVersion(szVersion);

	pcBoard->AddPower(16, PP_5V);

	pcBoard->AddPower(32, PP_Ground);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SetupStandard40PinBoard(CBoardPins* pcBoard, char* szName, char* szVersion)
{
	pcBoard->SetName(szName);
	pcBoard->SetVersion(szVersion);

	pcBoard->AddPower(3, PP_5V);
	pcBoard->AddPower(7, PP_Ground);
	pcBoard->AddPower(14, PP_Ground);

	pcBoard->AddPower(34, PP_Ground);
	pcBoard->AddPower(27, PP_Ground);
	pcBoard->AddPower(23, PP_5V);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SetupAlternate40PinBoard(CBoardPins* pcBoard, char* szName, char* szVersion)
{
	pcBoard->SetName(szName);
	pcBoard->SetVersion(szVersion);

	pcBoard->AddPower(20, PP_5V);

	pcBoard->AddPower(40, PP_Ground);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SetupStandard56PinBoard(CBoardPins* pcBoard, char* szName, char* szVersion)
{
	pcBoard->SetName(szName);
	pcBoard->SetVersion(szVersion);

	pcBoard->AddPower(3, PP_5V);
	pcBoard->AddPower(9, PP_Ground);
	pcBoard->AddPower(20, PP_Ground);

	pcBoard->AddPower(48, PP_Ground);
	pcBoard->AddPower(37, PP_Ground);
	pcBoard->AddPower(31, PP_5V);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SetupAlternate56PinBoard(CBoardPins* pcBoard, char* szName, char* szVersion)
{
	pcBoard->SetName(szName);
	pcBoard->SetVersion(szVersion);

	pcBoard->AddPower(28, PP_5V);

	pcBoard->AddPower(56, PP_Ground);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SetupStandard64PinBoard(CBoardPins* pcBoard, char* szName, char* szVersion)
{
	pcBoard->SetName(szName);
	pcBoard->SetVersion(szVersion);

	pcBoard->AddPower(3, PP_5V);
	pcBoard->AddPower(10, PP_Ground);
	pcBoard->AddPower(23, PP_Ground);

	pcBoard->AddPower(55, PP_Ground);
	pcBoard->AddPower(42, PP_Ground);
	pcBoard->AddPower(35, PP_5V);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SetupAlternate64PinBoard(CBoardPins* pcBoard, char* szName, char* szVersion)
{
	pcBoard->SetName(szName);
	pcBoard->SetVersion(szVersion);

	pcBoard->AddPower(32, PP_5V);

	pcBoard->AddPower(64, PP_Ground);
}

