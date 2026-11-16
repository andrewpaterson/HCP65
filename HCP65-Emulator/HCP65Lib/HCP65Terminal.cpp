#include "HCP65Terminal.h"


//////////////////////////////////////////////////////////////////////////
//
// Not attached, one empty line.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Terminal::Init(void)
{
	mpcUART = NULL;
	mcMutex.Init();
	macPending.Init();
	maszLines.Init();
	maszLines.Add("");
	muiColumn = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Terminal::Kill(void)
{
	Detach();
	maszLines.Kill();
	macPending.Kill();
	mcMutex.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// At the far end of pcUART's line (out of any other), with the far end's
// RTS and DTR on.  What was shown stays.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Terminal::Attach(CHCP65UART* pcUART)
{
	Detach();
	mpcUART = pcUART;
	mpcUART->SetHost(this);
	mpcUART->HostSetCTS(true);
	mpcUART->HostSetDSR(true);
	mpcUART->HostSetDCD(true);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Terminal::Detach(void)
{
	if (mpcUART && (mpcUART->GetHost() == this))
	{
		mpcUART->SetHost(NULL);
	}
	mpcUART = NULL;
}


CHCP65UART* CHCP65Terminal::GetUART(void) { return mpcUART; }


//////////////////////////////////////////////////////////////////////////
//
// On the HCP65's thread.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Terminal::Received(CHCP65UART* pcUART, uint8 uiCharacter)
{
	mcMutex.Lock();
	macPending.Add(uiCharacter);
	mcMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
// The characters received since into the lines.  True if there were any.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65Terminal::Update(void)
{
	CArrayUint8	acCharacters;
	size		i;

	acCharacters.Init();
	mcMutex.Lock();
	acCharacters.Add(macPending.GetData(), macPending.NumElements());
	macPending.ReInit();
	mcMutex.Unlock();

	for (i = 0; i < acCharacters.NumElements(); i++)
	{
		Put(acCharacters.GetValue(i));
	}
	i = acCharacters.NumElements();
	acCharacters.Kill();
	return i > 0;
}


//////////////////////////////////////////////////////////////////////////
//
// One character, as a teletype takes it.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Terminal::Put(uint8 uiCharacter)
{
	CChars*	pszLine;

	pszLine = maszLines.Tail();
	switch (uiCharacter)
	{
	case '\r':
		muiColumn = 0;
		return;

	case '\n':
		maszLines.Add("");
		muiColumn = 0;
		while (maszLines.NumElements() > HCP65_TERMINAL_MAX_LINES)
		{
			maszLines.Remove((size)0);
		}
		return;

	case '\b':
		if (muiColumn > 0)
		{
			muiColumn--;
		}
		return;

	case '\t':
		do
		{
			Put(' ');
		}
		while (muiColumn % HCP65_TERMINAL_TAB);
		return;

	case '\f':
		Clear();
		return;
	}

	if ((uiCharacter < ' ') || (uiCharacter > '~'))
	{
		return;
	}
	while ((size)pszLine->Length() < muiColumn)
	{
		pszLine->Append(' ');
	}
	if ((size)pszLine->Length() == muiColumn)
	{
		pszLine->Append((char)uiCharacter);
	}
	else
	{
		pszLine->SetChar(muiColumn, (char)uiCharacter);
	}
	muiColumn++;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Terminal::Clear(void)
{
	maszLines.ReInit();
	maszLines.Add("");
	muiColumn = 0;
}


size CHCP65Terminal::NumLines(void) { return maszLines.NumElements(); }
char* CHCP65Terminal::GetLine(size uiLine) { return maszLines.GetText(uiLine); }
size CHCP65Terminal::GetColumn(void) { return muiColumn; }


//////////////////////////////////////////////////////////////////////////
//
// The lines, with new lines between them.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Terminal::GetText(CChars* pszDest)
{
	size	i;

	pszDest->Clear();
	for (i = 0; i < maszLines.NumElements(); i++)
	{
		if (i > 0)
		{
			pszDest->Append('\n');
		}
		pszDest->Append(maszLines.GetText(i));
	}
}

