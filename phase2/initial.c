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

/**********************************************************
 *  main()
 *
 *  This function initializes system components
 *  required for process execution. It sets up the Pass Up
 *  Vector, initializes system data structures, creates the
 *  first process, and calls the scheduler.
 *
 *  Parameters:
 *         
 *
 *  Returns:
 *         
 *
 **********************************************************/
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
    LDIT(CLOCKINTERVAL);

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
    first_pro->p_s.s_status = ((IEPBITON) & KUPBITOFF) | TEBITON;
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

