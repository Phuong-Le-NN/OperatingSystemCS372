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
#include "/usr/include/umps3/umps/libumps.h"

extern void test();

/* Global Variables*/
int         process_count;           /* Number of started processes*/
int         softBlock_count;         /* Number of started that are in blocked*/
pcb_PTR     readyQ;                  /* Tail ptr to a queue of pcbs that are ready*/
pcb_PTR     currentP;   
/* Device Semaphores */

/* Pseudo Clock */
semd_t *pseudo_clock_sem;

/* 49 semaphores in an array */
int device_sem[49]; 

void main() {

    /* intializing semaphores */
    int i;
    for (i = 0; i < 49; i++){
        device_sem[i] = 0;
    }

    /* Nucleus TLB-Refill event Handler */
    passupvector_t *passup_pro0 = (memaddr)0x0FFFF900;
    passup_pro0->tlb_refll_handler = (memaddr) uTLB_RefillHandler;  /*uTLB RefillHandler replaced later*/
    passup_pro0->tlb_refll_stackPtr = (memaddr)0x20001000;

    currentP

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

    /* enable interrupt 
     enable Local Timer (TE) 
     turn on kernal mode  */

    /* If we did it incorrectly, this would be the one we do to CP0.
    */
    /* enable IEp and KUp in the status reg */
    first_pro->p_s.s_status = enable_IEp(first_pro->p_s.s_status);
    first_pro->p_s.s_status = kernel(first_pro->p_s.s_status);
    first_pro->p_s.s_status = enable_TE(first_pro->p_s.s_status);


    setSTATUS(0);
    int current_status_register = getSTATUS();

    /* after getting the current status register, to set in the IEp & KUp
    to desired assignment through bit-wise operation and then load it? */


    first_pro->p_s.s_pc = (memaddr) test;
    /*technical reasons, assign same value to both PC and general purpose register t9*/
    first_pro->p_s.s_reg[24] = (memaddr) test;

    passup_pro0->exception_stackPtr = (memaddr)RAMBASEADDR + RAMBASESIZE; 
    
    /* Call a function in Scheduler */
}

