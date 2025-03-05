/************************** INITIAL.H ******************************
*
*
*  Written by Phuong and Oghap on Feb 2025
*/

#ifndef INITIAL_H
#define INITIAL_H

#include "../h/pcb.h"

extern void test();

/* Global Variables*/
int         process_count;           /* Number of started processes */
int         softBlock_count;         /* Number of started that are in blocked */
pcb_PTR     readyQ;                  /* Tail ptr to a queue of pcbs that are ready */
pcb_PTR     currentP;                /* Current Process */
int device_sem[DEVINTNUM*DEVPERINT + DEVPERINT + 1];  /* Device Semaphores 49 semaphores in an array */

void main();
#endif
