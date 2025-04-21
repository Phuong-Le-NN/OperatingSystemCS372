#include "../h/pcb.h"
#include "delayDaemon.h"

#define MAXPROC 20
#define MAXSIGNEDINT 0x7FFFFFFF

HIDDEN delayd_t *delaydFree_h;
HIDDEN delayd_t delaydTable[MAXPROC + 2];
HIDDEN delayd_t *delayd_h;
HIDDEN int ADL_mutex;

void freeDelayd(delayd_t *d) {
	d->d_supStruct = NULL;
	d->d_next = delaydFree_h;
	d->d_wakeTime = 0;
	delaydFree_h = d;
}

delayd_t *allocDelayd(int wakeTime, support_t *currentSupport) {
	/*special case no free delayD to allocate*/
	if(delaydFree_h == NULL) {
		return NULL;
	}
	/*removing the first wakeTime in delaydFree_h*/
	delayd_t *allocatedDelayd = delaydFree_h;

	delaydFree_h = delaydFree_h->d_next;

	/*initialize its attributes*/
	allocatedDelayd->d_next = NULL;
	allocatedDelayd->d_wakeTime = wakeTime;
	allocatedDelayd->d_supStruct = currentSupport;

	return allocatedDelayd;
}

delayd_t *traverseADL(int wakeTime) {
	delayd_t *traverse = delayd_h;
	/*the loop that move traverse pointer until the the next wakeTime no longer has the descriptor smaller than the given wakeTime descriptor*/
	while((traverse->d_next->d_wakeTime != MAXSIGNEDINT) && (traverse->d_next->d_wakeTime < wakeTime)) {
		traverse = traverse->d_next;
	}
	return traverse;
}

void DELAY(support_t *currentSupport) {
	int wakeTime = currentSupport->sup_exceptState[GENERALEXCEPT].s_a1 * 1000000;

	if((wakeTime < 0) || (wakeTime > MAXSIGNEDINT)) {
		SYSCALL(9, 0, 0, 0);
	}

	SYSCALL(3, &ADL_mutex, 0, 0);
	delayd_t *newDelayd = allocDelayd(wakeTime, currentSupport);
	if(newDelayd == NULL) {
		SYSCALL(4, &ADL_mutex, 0, 0);
		SYSCALL(9, 0, 0, 0); /*fail to allocate*/
	}

	delayd_t *predecessor = traverseADL(wakeTime); /*look for the predecessor*/
	newDelayd->d_next = predecessor->d_next;
	predecessor->d_next = newDelayd;

	setSTATUS(getSTATUS() & (~IECBITON));
	SYSCALL(4, &ADL_mutex, 0, 0);

	SYSCALL(3, &(currentSupport->delaySem), 0, 0); /* this call scheduler and launch the next */
	setSTATUS(getSTATUS() | IECBITON);

	currentSupport->sup_exceptState[GENERALEXCEPT].s_pc += 4; /* after this proc is awoken*/
	LDST(&(currentSupport->sup_exceptState[GENERALEXCEPT]));  /* recheck what is the right state to load pc+4 ?*/
}

void delay_daemon() {
	int currTOD;
	delayd_t *to_be_wake;
	while(TRUE == TRUE) {
		SYSCALL(7, 0, 0, 0);

		SYSCALL(3, &ADL_mutex, 0, 0);
		STCK(currTOD);
		while(delayd_h->d_next->d_wakeTime <= currTOD) { /*break when the second node in the ADL no longer need to be wakened up*/ /* should delayDaemon only wake up up to when it start or when it finishes this iteration?*/
			to_be_wake = delayd_h->d_next;

			SYSCALL(4, &(to_be_wake->d_supStruct->delaySem), 0, 0);

			delayd_h->d_next = to_be_wake->d_next;
			freeDelayd(to_be_wake);

			STCK(currTOD);
		}
		SYSCALL(4, &ADL_mutex, 0, 0);
	}
}

void initADL() {
	delayd_h = NULL;
	ADL_mutex = 1;
	/*adding the first from delaydTable to the delaydFree_h list*/
	delaydFree_h = &(delaydTable[0]);
	int i;
	/*adding and connecting the rest wakeTime from delaydTable to the delaydFree_h list*/
	for(i = 0; i < MAXPROC + 1; i = i + 1) {
		(delaydTable[i]).d_next = &(delaydTable[i + 1]);
	}
	/*making the d_next pointer of the tail node NULL*/
	(delaydTable[i]).d_next = NULL;

	/*allocating and adding the two dummy node into the ADL*/
	delayd_t *headDummy = allocDelayd(-1, NULL);
	delayd_t *tailDummy = allocDelayd(MAXSIGNEDINT, NULL);

	headDummy->d_next = tailDummy;
	delayd_h = headDummy;

	state_t daemonState;
	daemonState.s_pc = (memaddr)delay_daemon;
	daemonState.s_t9 = (memaddr)delay_daemon;
	daemonState.s_sp = ((devregarea_t *)RAMBASEADDR)->rambase + ((devregarea_t *)RAMBASEADDR)->ramsize - PAGESIZE;
	daemonState.s_status = (IEPBITON & KUPBITOFF) | IPBITS;
	daemonState.s_entryHI = 0 << ASID_SHIFT;
	SYSCALL(1, &daemonState, NULL, 0);
}