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

int         process_count;      /* Number of started processes*/
int         softBlock_count;    /* Number of started that are in blocked*/
pcb_PTR     readyQ;             /* Tail ptr to a queue of pcbs that are ready*/
pcb_PTR     currentP;           /* Pointer to the pcb that is in the “running” state*/
/* Device Semaphore*/
