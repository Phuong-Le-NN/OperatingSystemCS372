/*********************************INITPROC.C*******************************
 *
 *  Implementation of the User Process Initialization Module
 *
 *  This module is responsible for initializing the user level support
 *  structures and page tables required for U-proc execution. It sets
 *  up the exception contexts, ASIDs, and program states for up to
 *  8 user processes, enabling virtual memory support and TLB handling.
 *
 *  The user process setup includes:
 *  - Initializing each U-proc’s private page table with proper VPN,
 *    ASID, and valid/dirty bits.
 *  - Assigning stack pointers and exception handlers for both TLB and
 *    general exceptions.
 *  - Creating the initial state for each process and invoking SYSCALL
 *    to create the PCB and insert it into the Ready Queue.
 *
 *      Modified by Phuong and Oghap on March 2025
 */

#include "initProc.h"
#include "vmSupport.h"
#include "sysSupport.h"
#include "../phase5/delayDaemon.h"

int masterSemaphore = 0;
int mutex[DEVINTNUM * DEVPERINT + DEVPERINT];

/**********************************************************
 *  init_Uproc_pgTable
 *
 *  Initializes the page table for a user-level process.
 *  Sets EntryHi with VPN and ASID, and EntryLo with initial
 *  D, V, and G bit settings. Stack page is set separately.
 *
 *  Parameters:
 *         support_t *currentSupport – pointer to U-proc's support structure
 *
 *  Returns:
 *
 **********************************************************/
void init_Uproc_pgTable(support_t *currentSupport) {
	/* To initialize a Page Table one needs to set the VPN, ASID, V, and D bit fields for each Page Table entry*/
	int i;
	for(i = 0; i < PAGE_TABLE_SIZE; i++) {
		/* ASID field, for any given Page Table, will all be set to the U-proc’s unique ID*/
		currentSupport->sup_privatePgTbl[i].EntryHi = KUSEG + (i * PAGESIZE) + (currentSupport->sup_asid << ASID_SHIFT);
		currentSupport->sup_privatePgTbl[i].EntryLo = (DBITON & GBITOFF) & VBITOFF;
	}
	/* reset the idx 31 element */
	currentSupport->sup_privatePgTbl[PAGE_TABLE_SIZE - 1].EntryHi = UPROC_STACK_AREA + (currentSupport->sup_asid << ASID_SHIFT);
}

/**********************************************************
 *  init_Uproc
 *
 *  Initializes a U-proc's state, support structure, and
 *  exception contexts. Also sets up its page table and
 *  creates a new PCB by calling SYS1.
 *
 *  Parameters:
 *         support_t *initSupportPTR – pointer to support struct
 *         int ASID – unique ID of the user process
 *
 *  Returns:
 *
 **********************************************************/
int init_Uproc(support_t *initSupportPTR, int ASID) {
	state_t initState;

	initState.s_pc = UPROCSTARTADDR;
	initState.s_t9 = UPROCSTARTADDR;
	initState.s_sp = UPROCSTACK;
	initState.s_status = ((IEPBITON | TEBITON) | IPBITS) | KUPBITON;

	initState.s_entryHI = (ASID << ASID_SHIFT);

	initSupportPTR->sup_asid = ASID;

	initSupportPTR->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr)TLB_exception_handler;
	initSupportPTR->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr)general_exception_handler;

	initSupportPTR->sup_exceptContext[PGFAULTEXCEPT].c_status = ((IEPBITON | TEBITON) | IPBITS) & KUPBITOFF;
	initSupportPTR->sup_exceptContext[GENERALEXCEPT].c_status = ((IEPBITON | TEBITON) | IPBITS) & KUPBITOFF;

	initSupportPTR->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = &initSupportPTR->sup_stackTlb[TLB_STACK_AREA];
	initSupportPTR->sup_exceptContext[GENERALEXCEPT].c_stackPtr = &initSupportPTR->sup_stackGen[GEN_EXC_STACK_AREA];

	initSupportPTR->delaySem = 0;

	init_Uproc_pgTable(initSupportPTR);

	int newPcbStat = SYSCALL(1, &initState, initSupportPTR, 0);
	return newPcbStat;
}

/**********************************************************
 *  test
 *
 *  Function for initializing the system test.
 *  - Initializes swap structures and mutexes
 *  - Sets 8 user processes with init_Uproc()
 *  - Waits for all user processes to finish
 *
 *  Parameters:
 *
 *
 *  Returns:
 *
 **********************************************************/
void test() {
	initSwapStruct();
	/*initADL();*/

	int i;
	for(i = 0; i < (DEVINTNUM * DEVPERINT + DEVPERINT); i++) {
		mutex[i] = 1;
	}

	support_t initSupportPTRArr[UPROC_NUM + 1]; /*1 extra sentinel node*/

	int newUprocStat;
	for(i = 1; i <= 1; i++) {
		newUprocStat = init_Uproc(&initSupportPTRArr[i - 1], i);
		if(newUprocStat == -1) {
			SYSCALL(TERMINATETHREAD, 0, 0, 0);
		}
	}

	for(i = 1; i <= 1; i++) {
		SYSCALL(PASSERN, &masterSemaphore, 0, 0); /* P operation */
	}

	SYSCALL(TERMINATETHREAD, 0, 0, 0);
}