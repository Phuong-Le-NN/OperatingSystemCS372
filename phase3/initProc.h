/************************** INITPROC.H ******************************
 *
 *  The externals declaration file for INITPROC Module
 *
 *  Written by Phuong and Oghap on Mar 2025
 */

#ifndef INITPROC_H
#define INITPROC_H

#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"

void test();
extern int mutex[DEVINTNUM * DEVPERINT + DEVPERINT];
extern int masterSemaphore;

#endif