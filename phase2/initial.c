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
#include "/usr/include/umps3/umps/libumps.h"

extern void test();

/* Global Variables*/
int         process_count;           /* Number of started processes*/
int         softBlock_count;         /* Number of started that are in blocked*/
pcb_PTR     readyQ;                  /* Tail ptr to a queue of pcbs that are ready*/
pcb_PTR     currentP;   
/* Device Semaphores */

/* Pseudo Clock */ /*remember to delete this*/
semd_t *pseudo_clock_sem;

/* 49 semaphores in an array */
int device_sem[49]; 

/* replace this with const !!!!*/
void uTLB_RefillHandler () {
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST ((state_PTR) 0x0FFFF000);
}

void exception_handler(){
    pcb_PTR callerProc = BIOSDATAPAGE; 
    /*
    The cause of this exception is encoded in the .ExcCode field of the Cause register
    (Cause.ExcCode) in the saved exception state. [Section 3.3-pops]
    • For exception code 0 (Interrupts), processing should be passed along to your
    Nucleus’s device interrupt handler. [Section 3.6]
    • For exception codes 1-3 (TLB exceptions), processing should be passed
    along to your Nucleus’s TLB exception handler. [Section 3.7.3]
    • For exception codes 4-7, 9-12 (Program Traps), processing should be passed
    along to your Nucleus’s Program Trap exception handler. [Section 3.7.2]
    • For exception code 8 (SYSCALL), processing should be passed along to
    your Nucleus’s SYSCALL exception handler. [Section 3.5]
    Hence, the entry point for the Nucleus’s exception handling is in essence a
    case statement that performs a multi-way branch depending on the cause of the
    exception.
    */
}

void main() {

    /* intializing semaphores */
    int i;
    int numberOfSemaphores = DEVINTNUM*DEVPERINT + DEVPERINT + 1;
    for (i = 0; i < numberOfSemaphores; i++){
        device_sem[i] = 0;
    }

    /* Nucleus TLB-Refill event Handler */
    passupvector_t *passup_pro0 = (memaddr) PASSUPVECTOR;
    passup_pro0->tlb_refll_handler = (memaddr) uTLB_RefillHandler;  /*uTLB RefillHandler replaced later*/
    passup_pro0->tlb_refll_stackPtr = (memaddr) (RAMSTART + PAGESIZE);

    /* exception_handler is the exception handler function */
    passup_pro0->exception_handler = (memaddr)exception_handler;

    /* Initialize pcbs and initASL*/
    initASL();
    initPcbs();

    process_count = 0;          
    softBlock_count = 0;  
    readyQ = mkEmptyProcQ();              
    currentP = NULL;

    /* Initialize all device semaphores to 0 */ /*Actually no need to do it here right? cause it is supposed to be in the*/
    pseudo_clock_sem = 0;
    /* the other devices semaphores as well */

    /* LDIT(T)	((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) */
    LDIT(100000);

    pcb_PTR first_pro = allocPcb();
    insertProcQ(&readyQ, first_pro);

    /* enable interrupt 
     enable Local Timer (TE) 
     turn on kernal mode  */

    /* If we did it incorrectly, this would be the one we do to CP0.
    */
    /* enable IEp and KUp in the status reg */
    first_pro->p_s.s_status = enable_IEp(first_pro->p_s.s_status);
    first_pro->p_s.s_status = kernel(first_pro->p_s.s_status);
    first_pro->p_s.s_status = enable_TE(first_pro->p_s.s_status);
    first_pro->p_s.s_pc = (memaddr) test;
    /*technical reasons, assign same value to both PC and general purpose register t9*/
    first_pro->p_s.s_t9 = (memaddr) test;


    setSTATUS(0);
    int current_status_register = getSTATUS();

    /* after getting the current status register, to set in the IEp & KUp
    to desired assignment through bit-wise operation and then load it? */

    passup_pro0->exception_stackPtr = (memaddr)RAMBASEADDR + RAMBASESIZE; 
    
    /* Call a function in Scheduler */
}

