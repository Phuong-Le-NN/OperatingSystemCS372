/************************** INITIAL.H ******************************
 *
 *  The externals declaration file for INITAL Module
 *
 *  Written by Phuong and Oghap on Feb 2025
 */

#ifndef INITIAL_H
#define INITIAL_H

#include "../h/pcb.h"

extern void test();

/* Global Variables*/
extern int process_count;                                     /* Number of started processes */
extern int softBlock_count;                                   /* Number of started that are in blocked */
extern pcb_PTR readyQ;                                        /* Tail ptr to a queue of pcbs that are ready */
extern pcb_PTR currentP;                                      /* Current Process */
extern int device_sem[DEVINTNUM * DEVPERINT + DEVPERINT + 1]; /* Device Semaphores 49 semaphores in an array */

extern void debug(pcb_PTR a0, int a1, int a2, int a3);

void main();
#endif
