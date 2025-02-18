/*********************************INITIAL.C*******************************
 *
 *      Initializes global data structures
 * 
 *      Modified by Phuong and Oghap on Feb 2025
 */

/* Need initProcQ and initASL*/

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "exceptions.c"

/* Global Variables*/
int         process_count;           /* Number of started processes*/
int         softBlock_count;         /* Number of started that are in blocked*/
pcb_PTR     readyQ;                  /* Tail ptr to a queue of pcbs that are ready*/
pcb_PTR     currentP;   
/* Device Semaphores */

/* Pseudo Clock */
semd_t *pseudo_clock_sem;

/* 16 semaphores for 8 devices as there can be up to 8 devices */
device_t; 

extern void test();

void main() {

    /* Nucleus TLB-Refill event Handler */
    passupvector_t *passup_pro0 = (memaddr)0x0FFFF900;
    passup_pro0->tlb_refll_handler = (memaddr) uTLB_RefillHandler;  /*uTLB RefillHandler replaced later*/
    passup_pro0->tlb_refll_stackPtr = (memaddr)0x20001000;

    /* fooBar is the exception handler function in exceptions.c */
    passup_pro0->exception_handler = (memaddr)fooBar;

    /* Initialize pcbs and initASL*/
    initASL();
    initPcbs();

    process_count = 0;          
    softBlock_count = 0;  
    readyQ = mkEmptyProcQ();              
    currentP = NULL;

    /* Initialize all device semaphores to 0 */
    pseudo_clock_sem = allocSemd(0);
    /* the other devices semaphores as well */

    /* LDIT(T)	((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) */
    LDIT(100);

    pcb_PTR first_pro = allocPcb();
    insertProcQ(&readyQ, first_pro);
    first_pro->p_s.s_pc = (memaddr) test;

    passup_pro0->exception_stackPtr = (memaddr)0x0; 
    
    /* Call Scheduler */
}

