/************************** EXCEPTIONS.H ******************************
 *
 *  The externals declaration file for EXCEPTIONS Module
 *
 *  Written by Phuong and Oghap on Feb 2025
 */

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

extern int process_count;                                     /* Number of started processes */
extern int softBlock_count;                                   /* Number of started that are in blocked */
extern pcb_PTR readyQ;                                        /* Tail ptr to a queue of pcbs that are ready */
extern pcb_PTR currentP;                                      /* Current Process */
extern int device_sem[DEVINTNUM * DEVPERINT + DEVPERINT + 1]; /* Device Semaphores 49 semaphores in an array */

extern void uTLB_RefillHandler();

void exception_handler();

#endif