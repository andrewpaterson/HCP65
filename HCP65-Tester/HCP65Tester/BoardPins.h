#ifndef __BOARD_PINS_H__
#define __BOARD_PINS_H__
#include "BaseLib/MapIntString.h"
#include "BaseLib/MapStringInt.h"
#include "BaseLib/MapIntInt.h"
#include "BaseLib/ArrayBit.h"


enum EPinDirection
{
	PD_Unknown,
	PD_Input,
	PD_Output,
};


enum EPinSignal
{
	PS_Unknown,
	PS_Normal,
	PS_Inverted,
};

enum EPowerPin
{
	PP_Unknown,
	PP_Ground,
	PP_5V,
};


class CBusPins
{
protected:
	CMapIntInt	mmBusOffsets;

public:
	void	Init(void);
	void	Kill(void);

	int		Get(int iPinNumber);
	void	Set(int iPinNumber, int iBusOffset);

	bool	StartIteration(SMapIterator* psIter, int* piPinNumber, int* piBusOffset);
	bool	Iterate(SMapIterator* psIter, int* piPinNumber, int* piBusOffset);
};


typedef CMapStringTemplate<CBusPins> CMapStringBusses;


class CBoardPins
{
protected:
	CChars				mszName;
	CChars				mszVersion;

	CMapIntString		mmPinSignals;
	CMapStringInt		mmPinNames;

	CArrayBit			msOutputs;
	CArrayBit			msInverted;

	CMapIntInt			mmPinPowers;
	CMapIntInt			mmPinNoConnects;

	CMapStringBusses	mmBusses;
	CMapIntString		mmPinBusses;

	size				miNumPins;
	bool				mbDone;

	CArrayBit			msWriteValues;
	CArrayBit			msReadValues;

public:
	void	Init(void);
	void	Kill(void);

	bool	Done(void);

	void	SetName(char* szName);
	void	SetVersion(char* szVersion);

	bool	ValidateCanAdd(size iPinNumber, char* szPinName = NULL);
	bool	AddSignal(size iPinNumber, char* szPinName, EPinDirection eDirection, EPinSignal eSignal);
	bool	AddPower(size iPinNumber, EPowerPin ePower);
	bool	AddNoCon(size iPinNumber);

	bool	GroupBus(char* szBusName, char* szPinName, int iBusOffset);

	bool	Set(char* szPinName, bool bValue);
	bool	Set(size iPinNumber, bool bValue);
	bool	SetBus(char* szBusName, uint32 uiBusValues);
	void	SetZero(void);

	void	GenerateOutput(CChars* psz);
	void	GenerateWrite(CChars* psz);
	void	GenerateLeft(CArrayBit* psArray, CChars* psz);
	void	GenerateRight(CArrayBit* psArray, CChars* psz);
	void	GeneratePower(CChars* psz);
	void	GenerateRead(CChars* psz);
	
	void	UpdateRead(CChars* pszReadResult);

	bool	IsGroundStyleD(void);
	bool	IsGroundStyleC(void);
	bool	Is5VStyleC(void);
	bool	Is5VStyleB(void);

	char*	GetPin(size iPinNumber);
	
	size	NumPins(void);

	bool	Get(char* szPinName);
	bool	Get(size iPinNumber);
	uint32	GetBus(char* szBusName);
	bool	GetWrite(size iPinNumber);
	uint32	GetBusWrite(char* szBusName);

	bool	PrintRead(CChars* psz, bool bOnlyActive);
	void	PrintWrite(CChars* psz, bool bOnlyActive);
	void	DumpRead(bool bOnlyActive);
	void	DumpWrite(bool bOnlyActive);

protected:
	void	PrintPin(CChars* psz, CArrayBit* pcBits, size iPinNumber);
	void	PrintPinActive(CChars* psz, CArrayBit* pcBits, size iPinNumber);
	bool	IsBusRead(CBusPins* pcBusPins);
	bool	IsBusWrite(CBusPins* pcBusPins);
	void	PrintBus(CChars* psz, CArrayBit* pcBits, CBusPins* pcBusPins, char* pszName);
};


#endif // __BOARD_PINS_H__

