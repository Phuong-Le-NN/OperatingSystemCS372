/************************** SYSSUPPORT.H ******************************
*
*  The externals declaration file for SYSSUPPORT Module
*
*  Written by Phuong and Oghap on Mar 2025
*/

#ifndef SYSSUPPORT_H
#define SYSSUPPORT_H


#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
  
#include "../phase3/initProc.h"

extern int masterSemaphore;
extern int mutex[48];
extern int swapPoolSema4;
extern swapPoolFrame_t swapPoolTable[8 * 2];

void general_exception_handler();

#endif