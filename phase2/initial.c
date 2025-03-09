/*********************************INITIAL.C*******************************
 *
 *  Implementation of the Initialization Handling Module
 * 
 *  This module is responsible for initializing global data structures
 *  used in the Nucleus. It sets up the process count, ready queue,
 *  device semaphores, and other essential system variables.
 *
 *  The initialization process includes:
 *  - Setting up the Pass Up Vector for handling exceptions.
 *  - Initializing the Active Semaphore List (ASL) and Process Queue.
 *  - Instantiating the first process and placing it in the Ready Queue.
 *  - Loading the system-wide Interval Timer.
 *  - Ensuring the system enters the scheduler for process execution.
 *
 *      Modified by Phuong and Oghap on Feb 2025
 */


#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"

#include "exceptions.h"
#include "scheduler.h"

#include "initial.h"


/* global variables*/
int         process_count;                                      /* Number of started processes */
int         softBlock_count;                                    /* Number of started that are in blocked */
pcb_PTR     readyQ;                                             /* Tail ptr to a queue of pcbs that are ready */
pcb_PTR     currentP;                                           /* Current Process */
int device_sem[DEVINTNUM*DEVPERINT + DEVPERINT + 1];            /* Device Semaphores 49 semaphores in an array */

extern void debug(pcb_PTR a0, int a1, int a2, int a3){

}


void main() {

    /* Populate the pass up vector */
    passupvector_t *passup_pro0 = (passupvector_t *) PASSUPVECTOR;
    passup_pro0->tlb_refll_handler = (memaddr) uTLB_RefillHandler; 
    /* Set the Stack Pointer for the Nucleus TLB-Refill event handler to the top of the Nucleus stack page */
    passup_pro0->tlb_refll_stackPtr = (memaddr) (RAMSTART + PAGESIZE);
    /* exception_handler is the exception handler function */
    passup_pro0->exception_handler = (memaddr) exception_handler;
    /* Stack pointer for the Nucleus exception handler to the top of the Nucleus stack page: 0x2000.1000. */
    passup_pro0->exception_stackPtr = (memaddr) (RAMSTART + PAGESIZE);
    

    /* Initialize pcbs and initASL*/
    initASL();
    initPcbs();

    /* Initialize all Nucleus maintained variables */
    process_count = 0;          
    softBlock_count = 0;  
    readyQ = mkEmptyProcQ();              
    currentP = NULL;

    /* Initalizing device semaphores to 0 */
    int i;
    int numberOfSemaphores = DEVINTNUM*DEVPERINT + DEVPERINT + 1;
    for (i = 0; i < numberOfSemaphores; i++){
        device_sem[i] = 0;
    }

    /* Load the system-wide Interval Timer with 100 milliseconds */
    LDIT(100000);

    /* Instantiate a single process, place its pcb in the Ready Queue, and increment Process Count. */
    pcb_PTR first_pro = allocPcb();
    insertProcQ(&readyQ, first_pro);
    process_count++;

    /*  Interrupts enabled
        Processor Local Timer enabled
        Kernel-mode on
        SP set to RAMTOP (i.e. use the last RAM frame for its stack)
        PC set to the address of test
    */
    first_pro->p_s.s_status = enable_IEp(first_pro->p_s.s_status);
    first_pro->p_s.s_status = kernel(first_pro->p_s.s_status);
    first_pro->p_s.s_status = enable_TE(first_pro->p_s.s_status);
    first_pro->p_s.s_pc = (memaddr) test;

    /*technical reasons, assign same value to both PC and general purpose register t9*/
    first_pro->p_s.s_t9 = (memaddr) test;
    first_pro->p_s.s_sp = ((devregarea_t *) RAMBASEADDR)->rambase + ((devregarea_t *) RAMBASEADDR)->ramsize;
    
    
    /*  Set all the Process Tree fields to NULL.
        Set the accumulated time field (p time) to zero.
        Set the blocking semaphore address (p semAdd) to NULL.
        Set the Support Structure pointer (p supportStruct) to NULL.

        COMPLETED in pcb.c */
    
    /* Call the Scheduler */
    scheduler();
}

