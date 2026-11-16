#ifndef __HCP65_VIA_BOARD_H__
#define __HCP65_VIA_BOARD_H__


//The HCP65 VIA Board (HCP65Lib/HCP65VIA.h has the board) through its driver, \OS\Drivers\VIA.DRV: open a VIA with
//KernalDeviceOpen(VIA_BOARD_MANUFACTURER, VIA_BOARD_NAME, index, unit, &handle) (HCP65User.h).  Up to eight W65C22S
//VIAs, two on each of the board's four VIA Breakouts, which are optional.  The units are the eight VIAs, named by
//their ports; a VIA whose breakout isn't fitted can't be opened (KERNAL_DEVICE_BAD_UNIT), and VIA_BOARD_GET_FITTED
//says which are.  Each VIA is opened on its own: different programs can have different VIAs.
//
//The VIAs' PHI2, which their timers count, is the board's clock halved: VIA_BOARD_PHI2_RATE.
//
//A VIA opens, and is left when it is closed, as the W65C22 is after reset: every pin an input, the timers' latches and
//counters as they were, ACR and PCR 0, no flags set and no interrupts enabled.
//
//KernalDeviceRead and KernalDeviceWrite move bytes between the program and the VIA's registers (VIA_*), from the
//register the offset is, one register a byte: offset plus length is at most VIA_NUM_REGISTERS.  They are the W65C22's
//own registers, with all that reading and writing them does (reading T1C-L clears T1's flag, writing ORA handshakes
//...).  But for IER, which is the driver's: writing it is KERNAL_DEVICE_BAD_ARGUMENT, and nothing is written.
//
//KernalDeviceWait's value is IFR flags (VIA_INT_*, 1 to $7F): it waits until any of them is set, enabling their
//interrupts while it waits.  It leaves the flags set: clear them as the W65C22 does (reading T1C-L, writing the IFR
//...).  A flag already set ends it at once.
#define VIA_BOARD_MANUFACTURER			"HCP65"
#define VIA_BOARD_NAME					"VIA Board"

//Units.
#define VIA_BOARD_AB					0		//Breakout 0.
#define VIA_BOARD_CD					1
#define VIA_BOARD_EF					2		//Breakout 1.
#define VIA_BOARD_GH					3
#define VIA_BOARD_XJ					4		//Breakout 2.
#define VIA_BOARD_KL					5
#define VIA_BOARD_MN					6		//Breakout 3.
#define VIA_BOARD_OP					7
#define VIA_BOARD_NUM_VIAS				8

#define VIA_BOARD_PHI2_RATE				12500000	//Hz, from the board's 25MHz.

//KernalDeviceControl codes.
#define VIA_BOARD_GET_FITTED			0		//Bit n set where unit n is fitted.

//A W65C22's registers: KernalDeviceRead's and KernalDeviceWrite's offsets.
#define VIA_ORB							0x0		//Read: IRB.
#define VIA_ORA							0x1		//Read: IRA.  Handshakes on CA2.
#define VIA_DDRB						0x2
#define VIA_DDRA						0x3
#define VIA_T1CL						0x4		//Write: T1's low latch.  Read: its counter's low byte, clearing its flag.
#define VIA_T1CH						0x5		//Write: T1's high latch, then the latches to the counter, starting it.
#define VIA_T1LL						0x6
#define VIA_T1LH						0x7		//Write: clears T1's flag.
#define VIA_T2CL						0x8		//Write: T2's low latch.  Read: its counter's low byte, clearing its flag.
#define VIA_T2CH						0x9		//Write: T2's counter high, and the low latch to its counter low, starting it.
#define VIA_SR							0xA
#define VIA_ACR							0xB
#define VIA_PCR							0xC
#define VIA_IFR							0xD
#define VIA_IER							0xE		//The driver's: it can only be read.
#define VIA_ORA_NH						0xF		//ORA without the handshake.
#define VIA_NUM_REGISTERS				16

//IFR's flags: KernalDeviceWait's value.
#define VIA_INT_CA2						0x01
#define VIA_INT_CA1						0x02
#define VIA_INT_SR						0x04
#define VIA_INT_CB2						0x08
#define VIA_INT_CB1						0x10
#define VIA_INT_T2						0x20
#define VIA_INT_T1						0x40
#define VIA_INT_FLAGS					0x7F
#define VIA_INT_ANY						0x80	//IFR: any enabled flag.  IER: set, rather than clear, the bits written.

//ACR.
#define VIA_ACR_PA_LATCH				0x01
#define VIA_ACR_PB_LATCH				0x02
#define VIA_ACR_SR_MASK					0x1C
#define VIA_ACR_SR_IN_T2				0x04
#define VIA_ACR_SR_IN_PHI2				0x08
#define VIA_ACR_SR_IN_CB1				0x0C
#define VIA_ACR_SR_OUT_FREE				0x10
#define VIA_ACR_SR_OUT_T2				0x14
#define VIA_ACR_SR_OUT_PHI2				0x18
#define VIA_ACR_SR_OUT_CB1				0x1C
#define VIA_ACR_T2_PULSES				0x20	//T2 counts PB6's falls.
#define VIA_ACR_T1_CONTINUOUS			0x40
#define VIA_ACR_T1_PB7					0x80	//T1 drives PB7.

//PCR.
#define VIA_PCR_CA1_POSITIVE			0x01
#define VIA_PCR_CA2_INPUT_NEGATIVE		0x00
#define VIA_PCR_CA2_INDEPENDENT_NEGATIVE	0x02
#define VIA_PCR_CA2_INPUT_POSITIVE		0x04
#define VIA_PCR_CA2_INDEPENDENT_POSITIVE	0x06
#define VIA_PCR_CA2_HANDSHAKE			0x08
#define VIA_PCR_CA2_PULSE				0x0A
#define VIA_PCR_CA2_LOW					0x0C
#define VIA_PCR_CA2_HIGH				0x0E
#define VIA_PCR_CB1_POSITIVE			0x10
#define VIA_PCR_CB2_INPUT_NEGATIVE		0x00
#define VIA_PCR_CB2_INDEPENDENT_NEGATIVE	0x20
#define VIA_PCR_CB2_INPUT_POSITIVE		0x40
#define VIA_PCR_CB2_INDEPENDENT_POSITIVE	0x60
#define VIA_PCR_CB2_HANDSHAKE			0x80
#define VIA_PCR_CB2_PULSE				0xA0
#define VIA_PCR_CB2_LOW					0xC0
#define VIA_PCR_CB2_HIGH				0xE0


#endif // __HCP65_VIA_BOARD_H__

