/*********************************ADL.C*******************************
 *  Implementation of the Active Semaphore List
 *      Maintains a sorted NULL-terminated single, linked
 *      lists of semaphore descriptors pointed to by pointer delayd_h
 *      Also maintains the delaydFree list, to hold the unused semaphore descriptors.
 *      For greater ADL traversal efficiency, we place
 *      a dummy node at both the head (s wakeTime ← 0) and
 *      tail (s wakeTime ← MAXINT) of the ADL
 *      This module includes public functions to support initializing ADL,
 *      insert into queue of a wakeTime in ADL using insertBlockec(),
 *      removing from head queue of a wakeTime in ADL using removeDelayed(),
 *      removing from anywhere in queue of a a wakeTime in ADL outDelayed(),
 *      accessing head of a queue of a wakeTime in ADL headDelayed().
 *      Some helper functions (not accessible publicly) are also implemented in this file.
 */

#include "../h/pcb.h"
#define MAXPROC 20

HIDDEN delayd_t *delaydFree_h;
static delayd_t delaydTable[MAXPROC + 2];
static delayd_t *delayd_h = NULL;
int ADL_mutex = 1;

/**********************************************************
 *  Cleaning the wakeTime before adding to the delaydFree_h
 *
 *  Parameters:
 *         delayd_t *p : pointer to the wakeTime to be free
 *
 *  Returns:
 *
 *
 */
void freeDelayd(delayd_t *d) {
	d->d_supStruct = NULL;
	d->d_next = delaydFree_h;
    d->d_wakeTime = 0;
	delaydFree_h = d;
}

/**********************************************************
 *  Initializing the wakeTime before after removing from the delaydFree_h list
 *
 *  Parameters:
 *         int* wakeTime :  the address to initialize for the new wakeTime
 *
 *  Returns:
 *         delayd_t *allocatedDelayd : pointer to the allocated wakeTime
 *
 */
delayd_t *allocDelayd(int *wakeTime, support_t *currentSupport) {
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

/**********************************************************
 *  A Helper Function: Traverse the ADL to look for the wakeTime with largest address
 *  that is smaller than the provided address of a wakeTime (possible pointer)
 *
 *  Parameters:
 *         int* wakeTime : the wakeTime descriptor to look for
 *
 *
 *  Returns:
 *         delayd_t *traverse : pointer to the possible predecessor wakeTime
 *
 */
delayd_t *traverseADL(int wakeTime) {
	delayd_t *traverse = delayd_h;
	/*the loop that move traverse pointer until the the next wakeTime no longer has the descriptor smaller than the given wakeTime descriptor*/
	while((traverse->d_next->d_wakeTime != 0xFFFFFFFF) && (traverse->d_next->d_wakeTime < wakeTime)) {
		traverse = traverse->d_next;
	}
	return traverse;
}

/**********************************************************
 *  Insert a provided pcb to a provided wakeTime
 *
 *  Parameters:
 *         int* wakeTime : the wakeTime descriptor to look for
 *         support_t *currentSupport : pointer to the currentSupport to delay
 *
 *
 *  Returns:
 *         TRUE if fail to insert due to unfound and unable to allocate wakeTime with descriptor wakeTime
 *         FALSE otherwise
 *
 */
int insertDelayed(int wakeTime, support_t *currentSupport) {
    if (wakeTime < 0){
        SYSCALL(9, 0, 0, 0);
    }

    SYSCALL(3, &ADL_mutex, 0, 0);
	delayd_t *newDelayd = allocDelayd(wakeTime, currentSupport);
    if(newDelayd == NULL){
        SYSCALL(4, &ADL_mutex, 0, 0);
        SYSCALL(9, 0, 0, 0);                  /*fail to allocate*/
    }

    delayd_t *predecessor = traverseADL(wakeTime);      /*look for the predecessor*/
    newDelayd->d_next = predecessor->d_next;
	predecessor->d_next = newDelayd;
    SYSCALL(4, &ADL_mutex, 0, 0);

    SYSCALL(3, &(currentSupport->delaySem), 0, 0);
	return FALSE;
}

/**********************************************************
 *  Removing from head queue of a wakeTime in ADL and remove wakeTime from ADL if no longer active
 *
 *  Parameters:
 *         int* wakeTime : the wakeTime descriptor to look for
 *
 *
 *  Returns:
 *         NULL if fail to remove
 *         pcb_PTR resultPbc : pointer to the removed pcb
 *
 */
void removeDelayed(int wakeTime) {
    delayd_t *to_be_wake;
    SYSCALL(3, &ADL_mutex, 0, 0);
    while (delayd_h->d_next->d_wakeTime <= wakeTime){   /*break when the second node in the ADL no longer need to be wakedn up*/
        to_be_wake = delayd_h->d_next;

        SYSCALL(4, &(to_be_wake->d_supStruct->delaySem), 0, 0);

        delayd_h->d_next = to_be_wake->d_next;
        freeDelayd(to_be_wake);
	}
    SYSCALL(4, &ADL_mutex, 0, 0);
	return;
}

/**********************************************************
 *  Filling the semaFree_h list with wakeTime from delaydTable
 *  Adding 2 dummy nodes into the delayd_h list using allocDelayd()
 *
 *  Parameters:
 *
 *
 *  Returns:
 *
 *
 */
void initADL() {
	/*adding the first wakeTime from delaydTable to the delaydFree_h list*/
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
	delayd_t *tailDummy = allocDelayd(0xFFFFFFFF, NULL);

	headDummy->d_next = tailDummy;
	delayd_h = headDummy;
}

void initDelayDaemon(support_t *currentSupport){
    state_t daemonState;
    daemonState.s_pc = (memaddr) delay_daemon;
    daemonState.s_t9 = (memaddr) delay_daemon;
    daemonState.s_sp = (memaddr)(RAMSTART + 2*PAGESIZE);
    daemonState.s_status = (IEPBITON & KUPBITOFF)|IPBITS;
    daemonState.s_entryHI = 0 << ASID_SHIFT;
    SYSCALL(1, &daemonState, NULL, 0);
}

void DELAY(){
    support_t *currentSupport = (support_t *) SYSCALL(8, 0, 0, 0);
    insertDelayed(currentSupport->sup_exceptState[GENERALEXCEPT].s_s2, currentSupport);
    LDST(&(currentSupport->sup_exceptState[GENERALEXCEPT]));
}

void delay_daemon(){
    SYSCALL(7, 0, 0, 0);
    int currentTOD = SYSCALL(10, 0, 0, 0);
    removeDelayed(currentTOD);
}