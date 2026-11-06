#ifndef __DEVICE_ROM_SCHEMATIC_H__
#define __DEVICE_ROM_SCHEMATIC_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"
#include "DeviceROM.h"


class CXMLWriter;
class CKicadSchemFile;
class CKicadSchemNetlist;
class CKicadSchemSymbol;
class CKicadSchemNet;


//What a board's schematic can't show is given as properties of its device ROM chip (the chip whose ~{CE} or ~{CS}
//is Device Decode Stage A's ~{Device_ROM}).  ROM_Manufacturer must be given; ROM_Name and ROM_Revision default to the
//title block's title and revision ("V0" is 0); ROM_Driver defaults to none.
#define DEVICE_ROM_PROPERTY_NAME			"ROM_Name"
#define DEVICE_ROM_PROPERTY_MANUFACTURER	"ROM_Manufacturer"
#define DEVICE_ROM_PROPERTY_REVISION		"ROM_Revision"
#define DEVICE_ROM_PROPERTY_DRIVER			"ROM_Driver"

//The HCP65 library symbols and bus signals the schematic is read by.
#define DEVICE_ROM_STAGE_A_SYMBOL			"Device_Decode_Stage_A"
#define DEVICE_ROM_STAGE_B_SYMBOL			"Device_Decode_Stage_B"
#define DEVICE_ROM_BUFFER_SYMBOL			"Device_Buffer"
#define DEVICE_ROM_MAIN_ACCESS_NET			"~{Main Access}"
#define DEVICE_ROM_MEMORY_A_NET				"~{Memory A}"
#define DEVICE_ROM_MEMORY_B_NET				"~{Memory B}"


//A register found on the schematic, named by the net its strobe drives.
struct SDeviceROMSchematicRegister
{
	uint16	uiOffset;
	uint8	uiDirection;		//DEVICE_REGISTER_*.
	char	szName[DEVICE_ROM_NAME_LENGTH];
};

typedef CArrayTemplate<SDeviceROMSchematicRegister>	CArrayDeviceROMSchematicRegister;


//A board's <DeviceROM> description (DeviceROM.h) worked out from its KiCad schematic (one sheet, its boards drawn as
//HCP65 library symbols):
//
//	Registers:	each Device Decode Stage B decodes eight registers from $400 + 8 * group, the group being the A3 and
//				A4 (or ~{A3}, ~{A4}, GND and 5V) on its ~{E2} and E3.  Each of its Yn_{R} and Yn_{W} that is wired to
//				something is a read or write register at n, named by the net it's on ("Write Flags" is Flags).
//	Device bus:	128KB if A16 goes anywhere but Stage A and the Device Buffer, otherwise 32KB.
//	Main bus:	Master if ~{Main Access} goes past the connector; Window A and B if ~{Memory A} and ~{Memory B} do.
class CDeviceROMSchematic
{
protected:
	CChars								mszName;
	CChars								mszManufacturer;
	CChars								mszDriver;
	uint8								muiRevision;
	uint8								muiDeviceBus;
	uint8								muiMainBus;
	CArrayDeviceROMSchematicRegister	masRegisters;

	CKicadSchemFile*					mpcFile;
	CKicadSchemNetlist*					mpcNetlist;
	CChars*								mpszError;

public:
	void			Init(void);
	void			Kill(void);

	bool			Read(const char* szSchematic, CChars* pszError);
	void			SaveXML(CXMLWriter* pcWriter);
	bool			WriteXML(const char* szFileName);

	char*			GetName(void);
	char*			GetManufacturer(void);
	char*			GetDriver(void);
	uint8			GetRevision(void);
	uint8			GetDeviceBus(void);
	uint8			GetMainBus(void);
	size			NumRegisters(void);
	SDeviceROMSchematicRegister*	GetRegister(size uiIndex);

protected:
	bool				Analyse(void);
	bool				Error(const char* szMessage, const char* szDetail);
	CKicadSchemSymbol*	FindSymbol(const char* szLibSymbol, size* puiCount);
	bool				IsSymbol(CKicadSchemSymbol* pcSymbol, const char* szLibSymbol);
	CKicadSchemNet*		GetPinNet(CKicadSchemSymbol* pcSymbol, const char* szPinName);
	bool				IsPowerNet(CKicadSchemNet* pcNet, int* piLevel);
	bool				ReachesOther(CKicadSchemNet* pcNet, CKicadSchemSymbol* pcExcept1, CKicadSchemSymbol* pcExcept2, bool bExceptConnectors);
	bool				AddressCondition(CKicadSchemNet* pcNet, int iActiveLevel, int* aiBits);
	bool				ReadStageB(CKicadSchemSymbol* pcStageB);
	bool				AddRegister(uint16 uiOffset, uint8 uiDirection, CKicadSchemNet* pcNet);
	bool				ReadDescription(CKicadSchemSymbol* pcStageA);
	void				ReadBuses(CKicadSchemSymbol* pcStageA);
};


#endif // __DEVICE_ROM_SCHEMATIC_H__
