#ifndef DEVSUPPORT_H
#define DEVSUPPORT_H

#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"

extern int masterSemaphore;
extern int mutex[DEVINTNUM * DEVPERINT + DEVPERINT];

void WRITE_TO_DISK(support_t *currentSupport);
void READ_FROM_DISK(support_t *currentSupport);
void READ_FROM_FLASH(support_t *currentSupport);
void WRITE_TO_FLASH(support_t *currentSupport);

#endif