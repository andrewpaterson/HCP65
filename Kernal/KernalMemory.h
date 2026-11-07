#ifndef __KERNAL_MEMORY_H__
#define __KERNAL_MEMORY_H__
#include "BaseLib/PrimitiveTypes.h"


//The kernal's memory.  Its heap (malloc) is MPU RAM's bank 6, below the remap table: it is always there.  Drivers go in
//MPU RAM from the end of the kernal's bss up to it.  Main memory is the memory boards' (Website docs/hcp65/devices.md,
//Main memory): each one found gets the next window, 1, 2, 3 then 0, through the main memory channel's registers.  Of
//the banks they give, the kernal keeps KERNAL_NUM_TABLE_BANKS for the maths tables and one bank for its main memory
//heap (KernalMainMalloc: what the SD Card Board's DMA must reach), from $200000 up, where kernel mode reaches main
//memory; processes get the rest.  With one board (window 1) the tables are banks $40 to $43, the heap $44 and
//processes $45 to $7F.
#define KERNAL_MPU_HEAP_START		0x060000
#define KERNAL_MPU_HEAP_END			0x070000	//KERNAL_REMAP_TABLE.
#define KERNAL_MAIN_MEMORY_BANK		0x20		//Kernel mode reaches main memory from here ($200000).
#define KERNAL_NUM_BANKS			256
#define KERNAL_NUM_TABLE_BANKS		4			//The maths tables boot.exe loads (UserMathTables.asm: MathMult 128KB, then
#define KERNAL_TABLES_USER_BANK		0x40		//INVERT's upper half 128KB), in every process at $400000.
#define KERNAL_MAIN_HEAP_SIZE		0x10000		//One bank.

#define KERNAL_MEMORY_CHANNEL		0x108000	//Device 1's registers (written directly, not decoded).

//What a physical main memory bank is (gauiKernalBanks).
#define KERNAL_BANK_ABSENT			0			//No chip answers it (or it is bank 0, which a remap page can't name).
#define KERNAL_BANK_PROCESS			1			//For processes.
#define KERNAL_BANK_TABLES			2
#define KERNAL_BANK_HEAP			3


extern uint8	gauiKernalBanks[KERNAL_NUM_BANKS];
extern uint8	gauiKernalTableBanks[KERNAL_NUM_TABLE_BANKS];


extern "C" void	KernalInitHeap(void);
bool			KernalInitMainMemory(void);
uint16			KernalNumMainBanks(void);
void*			KernalMainMalloc(uint32 uiBytes);
void			KernalMainFree(void* pv);
uint32			KernalMainHeapPeak(void);


#endif // __KERNAL_MEMORY_H__
