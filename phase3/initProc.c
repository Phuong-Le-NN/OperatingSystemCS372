/*********************************INITPROC.C*******************************
 * 
 *  This module implements test and exports the Support Level’s global 
 *  variables. 
 *
 *      Modified by Phuong and Oghap on March 2025
 */


#include "initProc.h"

int masterSemaphore = 0;

/**********************************************************
 *  
 **********************************************************/
void init_Uproc_pgTable() { /* 4.2.1 Pandos - A U-proc’s Page Table*/
    support_t *currentSupport = SYSCALL(8, 0, 0, 0);
    /* To initialize a Page Table one needs to set the VPN, ASID, V, and D bit fields for each Page Table entry*/
    int i;
    for (i = 0; i < 32; i ++){
        /* VPN field will be set to [0x80000..0x8001E] for the first 31 entries => should set up to idx 30 only => will have to reset the idx 31 element after the loop*/
        /* ASID field, for any given Page Table, will all be set to the U-proc’s unique ID*/
        currentSupport->sup_privatePgTbl[i].EntryHi =  0x80000000 + i*0x1000 + currentSupport->sup_asid << 6;
        /*  D bit field will be set to 1 (on) - bit 10 to 1*/ /* G bit field will be set to 0 (off) - bit 8 to 0*/ /* V bit field will be set to 0 (off) - bit 9 to 0*/
        currentSupport->sup_privatePgTbl[i].EntryLo = currentSupport->sup_privatePgTbl[i].EntryLo | 0x00000400 & 0xFFFFFEFF & 0xFFFFFDFF;
    }
    /* reset the idx 31 element -- VPN for the stack page entryHi (Page Table entry 31) should be set to 0xBFFFF*/
    currentSupport->sup_privatePgTbl[i].EntryHi =  0xBFFFF000 + currentSupport->sup_asid;
}

pcb_PTR init_Uproc(support_t *initSupportPTR, int ASID){
    state_t initState;

    initState.s_pc = 0x800000B0;
    initState.s_t9 = 0x800000B0;
    initState.s_sp = 0xC0000000;
    initState.s_cause = initState.s_cause | IEPBITON | TEBITON | IPBITS | KUPBITON;

    initState.s_entryHI = (ASID << 6) & 0x000003C0;

    initSupportPTR->sup_asid = ASID;

    initSupportPTR->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) TLB_exception_handler;
    initSupportPTR->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) general_exception_handler;

    initSupportPTR->sup_exceptContext[PGFAULTEXCEPT].c_status = IEPBITON | TEBITON | IPBITS & KUPBITOFF;
    initSupportPTR->sup_exceptContext[GENERALEXCEPT].c_status = IEPBITON | TEBITON | IPBITS & KUPBITOFF;

    initSupportPTR->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr =  &initSupportPTR->sup_stackTlb[499];
    initSupportPTR->sup_exceptContext[GENERALEXCEPT].c_stackPtr =  &initSupportPTR->sup_stackGen[499];

    init_Uproc_pgTable(initSupportPTR);

    pcb_PTR newPcb = SYSCALL(1, &initState, initSupportPTR, 0);
    return newPcb;
}

void test() {
    initSwapStruct();

    int i;
    for (i = 0; i < 48; i++){
        mutex[i] = 1;
    }

    support_t initSupportPTRArr[8];

    pcb_PTR newUproc;
    for (i = 1; i <= 1; i++){
        newUproc = init_Uproc(&initSupportPTRArr[i-1], i);
    }

    for (i = 1; i <= 1; i++){
        SYSCALL(3, &masterSemaphore, 0, 0); /* P operation */
    }

    SYSCALL(2, 0, 0, 0);
}