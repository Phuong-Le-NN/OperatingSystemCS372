/************************* INTERRUPTS.H *****************************
*
*  The externals declaration file for INTERRUPTS Module
*
*  Written by Phuong and Oghap
*/

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "../h/pcb.h"

extern int         process_count;           /* Number of started processes */
extern int         softBlock_count;         /* Number of started that are in blocked */
extern pcb_PTR     readyQ;                  /* Tail ptr to a queue of pcbs that are ready */
extern pcb_PTR     currentP;                /* Current Process */
extern int device_sem[DEVINTNUM*DEVPERINT + DEVPERINT + 1];  /* Device Semaphores 49 semaphores in an array */

void interrupt_exception_handler();

#endif