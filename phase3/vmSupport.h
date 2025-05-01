/************************** VMSUPPORT.H ******************************
 *
 *  The externals declaration file for VMSUPPORT Module
 *
 *  Written by Phuong and Oghap on Mar 2025
 */

#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"

/* global variables */
extern swapPoolFrame_t swapPoolTable[SWAP_POOL_SIZE];
extern int swapPoolSema4;

void initSwapStruct();
void uTLB_RefillHandler();
void TLB_exception_handler();

#endif