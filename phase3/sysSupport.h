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

void general_exception_handler();
void program_trap_handler(support_t *passedUpSupportStruct, semd_t *heldSemd);

#endif