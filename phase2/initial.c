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

/* Global Variables*/

void main() {
    int         process_count = 0;                          /* Number of started processes*/
    int         softBlock_count = 0;                        /* Number of started that are in blocked*/
    pcb_PTR     readyQ = mkEmptyProcQ();                    /* Tail ptr to a queue of pcbs that are ready*/
    xxx->tlb_refll_handler = (memaddr) uTLB_RefillHandler;  /*uTLB RefillHandler replaced later*/
    pcb_PTR     currentP = NULL;                            /* Pointer to the pcb that is in the “running” state*/
    /* Device Semaphore*/
}

