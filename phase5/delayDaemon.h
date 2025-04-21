/************************** SYSSUPPORT.H ******************************
 *
 *  The externals declaration file for SYSSUPPORT Module
 *
 */

#ifndef DELAYDAEMON_H
#define DELAYDAEMON_H

#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"

void initADL();
void DELAY(support_t *currentSupport);

#endif