/************************** VMSUPPORT.H ******************************
*
*  The externals declaration file for VMSUPPORT Module
*
*  Written by Phuong and Oghap on Mar 2025
*/

#ifndef VMSUPPORT_H
#define VMSUPPORT_H


#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"

extern int device_sem[DEVINTNUM*DEVPERINT + DEVPERINT + 1];  /* Device Semaphores 49 semaphores in an array */
extern void program_trap_handler();

/* global variables */
swapPoolFrame_t swapPoolTable[8 * 2];
int swapPoolSema4;

void initSwapStruct();
void uTLB_RefillHandler();
void TLB_exception_handler();

#endif