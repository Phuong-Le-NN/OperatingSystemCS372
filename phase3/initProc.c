/*********************************INITPROC.C*******************************
 * 
 *  This module implements test and exports the Support Level’s global 
 *  variables. 
 *
 *      Modified by Phuong and Oghap on March 2025
 */


#include "initProc.h"

int mutex[48];
int masterSemaphore = 0;

pcb_PTR init_Uproc(support_t *initSupport, int ASID){
    state_t initState;

    initState.s_pc = 0x800000B0;
    initState.s_t9 = 0x800000B0;
    initState.s_sp = 0xC0000000;
    initState.s_cause = initState.s_cause | IEPBITON | TEBITON | IPBITS | KUPBITON;

    initState.s_entryHI = (ASID << 6) & 0x000003C0;

    initSupport->sup_asid = ASID;

    initSupport->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) TLB_exception_handler;
    initSupport->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) general_exception_handler;

    initSupport->sup_exceptContext[PGFAULTEXCEPT].c_status = IEPBITON | TEBITON | IPBITS | KUPBITOFF;
    initSupport->sup_exceptContext[GENERALEXCEPT].c_status = IEPBITON | TEBITON | IPBITS | KUPBITOFF;

    initSupport->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr =  &initSupport->sup_stackGen[499];
    initSupport->sup_exceptContext[GENERALEXCEPT].c_stackPtr =  &initSupport->sup_stackGen[499];

    pcb_PTR newPcb = SYSCALL(1, &initState, initSupport, 0);
    return newPcb;
}

void test() {
    initSwapStruct();

    int i;
    for (i = 0; i < 48; i++){
        mutex[i] = 1;
    }

    support_t initSupportArr[8];

    pcb_PTR newUproc;
    for (i = 1; i <= 8; i++){
        newUproc = init_Uproc(&initSupportArr[i-1], i);
        SYSCALL(3, &masterSemaphore, 0, 0); /* P operation */
    }
}