/*********************************INITIAL.C*******************************
 *
 *      Initializes global data structures
 * 
 *      Modified by Phuong and Oghap on Feb 2025
 */

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "/usr/include/umps3/umps/libumps.h"
#include "exceptions.h"


extern void test();

/* Global Variables*/
int         process_count;           /* Number of started processes */
int         softBlock_count;         /* Number of started that are in blocked */
pcb_PTR     readyQ;                  /* Tail ptr to a queue of pcbs that are ready */
pcb_PTR     currentP;                /* Current Process */
int device_sem[DEVINTNUM*DEVPERINT + DEVPERINT + 1];  /* Device Semaphores 49 semaphores in an array */



/*
The cause of this exception is encoded in the .ExcCode field of the Cause register
(Cause.ExcCode) in the saved exception state. [Section 3.3-pops]

Hence, the entry point for the Nucleus’s exception handling 
that performs a multi-way branch depending on the cause of the
exception.
*/
void exception_handler(){
    /*load the saved state into the current Process*/
    deep_copy_state_t(&currentP->p_s, BIOSDATAPAGE);

    /* Get the Cause registers from the saved exception state and 
    use AND bitwise operation to get the .ExcCode field */
    int ExcCode = CauseExcCode(currentP->p_s.s_cause);

    if (ExcCode == 0){

        /* Call Nucleus’s device interrupt handler */
        interrupt_exception_handler();
    }
    else if (ExcCode <= 3)
    {
        /* Nucleus’s TLB exception handler */
        /* NEED TO CHECK */
        pass_up_or_die(PGFAULTEXCEPT);
    }
    else if (ExcCode <= 7 || ExcCode >= 9)
    {
        /* Nucleus’s Program Trap exception handler */
        pass_up_or_die(GENERALEXCEPT);
    }
    else
    {
        /*  SYSCALL exception handler */
        SYSCALL_handler();
    }
}

void main() {

    /* Nucleus TLB-Refill event Handler */
    /* Populate the pass up vector */
    passupvector_t *passup_pro0 = (memaddr) PASSUPVECTOR;
    passup_pro0->tlb_refll_handler = (memaddr) uTLB_RefillHandler;  /* uTLB RefillHandler replaced later */
    /* Set the Stack Pointer for the Nucleus TLB-Refill event handler to the top of the Nucleus stack page */
    passup_pro0->tlb_refll_stackPtr = (memaddr) (RAMSTART + PAGESIZE);
    /* exception_handler is the exception handler function */
    passup_pro0->exception_handler = (memaddr)exception_handler;
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

    /* intializing device semaphores to 0 */
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

    /* interrupts enabled
    the processor Local Timer enabled
    kernel-mode on
    the SP set to RAMTOP (i.e. use the last RAM frame for its stack)
    PC set to the address of test*/

    /* If we did it incorrectly, this would be the one we do to CP0. */
    /* enable IEp and KUp in the status reg */
    first_pro->p_s.s_status = enable_IEp(first_pro->p_s.s_status);
    first_pro->p_s.s_status = kernel(first_pro->p_s.s_status);
    first_pro->p_s.s_status = enable_TE(first_pro->p_s.s_status);
    first_pro->p_s.s_pc = (memaddr) test;
    /*technical reasons, assign same value to both PC and general purpose register t9*/
    first_pro->p_s.s_t9 = (memaddr) test;
    first_pro->p_s.s_sp = (memaddr) (RAMSTART + PAGESIZE);


    /* Set all the Process Tree fields to NULL.
    Set the accumulated time field (p time) to zero.
    Set the blocking semaphore address (p semAdd) to NULL.
    Set the Support Structure pointer (p supportStruct) to NULL. 
    completed in pcb.c */
    
    /* Call the Scheduler */
    scheduler();
}

