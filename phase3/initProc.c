/*********************************INITPROC.C*******************************
 * 
 *  This module implements test and exports the Support Level’s global 
 *  variables. 
 *
 *      Modified by Phuong and Oghap on March 2025
 */


#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
 
#include "../phase2/initial.h"
#include "../phase2/scheduler.h"
#include "../phase2/exceptions.h"
#include "../phase2/interrupts.h"

extern TLB_exception_handler;
extern general_exception_handler;

int mutex[48];
int masterSemaphore = 0;

void test() {
    initSwapStruct();

    int i;
    for (i = 0; i < 48; i++){
        mutex[i] = 1;
    }

    state_t initState;

    initState.s_pc = 0x800000B0;
    initState.s_t9 = 0x800000B0;
    initState.s_sp = 0xC0000000;
    initState.s_cause = initState.s_cause | IEPBITON | TEBITON | IPBITS | KUPBITON; 

    support_t initSupportArr[8];
    for (i = 1; i <= 8; i++){
        initSupportArr[i-1].sup_asid = i;

        initSupportArr[i-1].sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) TLB_exception_handler;
        initSupportArr[i-1].sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) general_exception_handler;

        initSupportArr[i-1].sup_exceptContext[PGFAULTEXCEPT].c_status = IEPBITON | TEBITON | IPBITS | KUPBITOFF;
        initSupportArr[i-1].sup_exceptContext[GENERALEXCEPT].c_status = IEPBITON | TEBITON | IPBITS | KUPBITOFF;

        initSupportArr[i-1].sup_exceptContext[PGFAULTEXCEPT].c_stackPtr =  &initSupportArr[i-1].sup_stackGen[499];
        initSupportArr[i-1].sup_exceptContext[GENERALEXCEPT].c_stackPtr =  &initSupportArr[i-1].sup_stackGen[499];
    }

    pcb_PTR newUproc;
    for (i = 1; i <= 8; i++){
        newUproc = init_Uproc(initState, &initSupportArr[i-1]);
        newUproc->p_s.s_entryHI = (i << 6) & 0x000003c0;

        SYSCALL(3, &masterSemaphore, 0, 0); /* P operation */
    }
    SYSCALL(2, 0, 0, 0);
}

pcb_PTR init_Uproc(state_t initState, support_t *initSupport){
    SYSCALL(1, &initState, initSupport, 0);
}