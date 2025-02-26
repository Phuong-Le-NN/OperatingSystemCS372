/*********************************EXCEPTIONS.C*******************************
 *
 * 
 *      Modified by Phuong and Oghap on Feb 2025
 */


#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "initial.c"
#include "scheduler.c"

/*

    - For exception code 0 (Interrupts), processing should be passed along to your 
        Nucleus’s device interrupt handler. [Section 3.6]

    - For exception codes 1-3 (TLB exceptions), processing should be passed
        along to your Nucleus’s TLB exception handler. [Section 3.7.3]

    - For exception codes 4-7, 9-12 (Program Traps), processing should be passed 
        along to your Nucleus’s Program Trap exception handler. [Section 3.7.2]

    - For exception code 8 (SYSCALL), processing should be passed along to
        your Nucleus’s SYSCALL exception handler. [Section 3.5]

*/



/*
    The process requesting the SYS1 service continues to exist and to execute.
    If the new process cannot be created due to lack of resources (e.g. no more free
    pcb’s), an error code of -1 is placed/returned in the caller’s v0, otherwise, return
    the value 0 in the caller’s v0.
*/
int CREATEPROCESS(){

    /*
    initializes:
        p s from a1.
        p supportStruct from a2. If no parameter is provided, this field is set
            to NULL.
        The process queue fields (e.g. p next) by the call to insertProcQ
        The process tree fields (e.g. p child) by the call to insertChild.
        p time is set to zero; the new process has yet to accumulate any cpu time.
        p semAdd is set to NULL; this pcb/process is in the “ready” state, not the
            “blocked” state.

        The newly populated pcb is placed on the Ready Queue and is made a child of
        the Current Process. Process Count is incremented by one, and control is returned
        to the Current Process.
    */

    pcb_PTR newProcess = allocPcb();
    if (newProcess == NULL) {
        currentP->p_s.s_v0 = -1;
        return -1;
    }
    newProcess->p_s = *((state_PTR) (currentP->p_s.s_a1));
    newProcess->p_supportStruct = ((state_PTR) (currentP->p_s.s_a2));
    insertProcQ(readyQ, newProcess);
    insertChild(currentP, newProcess);
    newProcess->p_time = 0;
    newProcess->p_semAdd = NULL;

    /* how can we check if there is no parameter provided */
    currentP->p_s.s_v0 = 0;
    return 0;
}

int TERMINATEPROCESS(pcb_PTR toBeTerminate){
    /*are we terminating the current process, then where are we supposed to put the return value in - a2 of currentP ???*/

    /* recursively, all progeny of this process are terminated as well. */
    while (!emptyChild(toBeTerminate)) {
        pcb_PTR childToBeTerminate = removeChild(currentP);
        TERMINATEPROCESS(childToBeTerminate);
    }
    outChild(toBeTerminate);
    freePcb(toBeTerminate);
    return;
}

int PASSEREN(semd_t *sema4){ //use and update a1
    /*  
        Depending on the value of the semaphore, control is either returned to the
        Current Process, or this process is blocked on the ASL (transitions from “running”
        to “blocked”) and the Scheduler is called.
    */

    sema4->s_semAdd --; /*what is supposed to be this sema4*/
    if (sema4->s_semAdd < 0){
        insertBlocked(sema4->s_semAdd, currentP);
        scheduler();
    }
    LDST(currentP);
    /*what to return here*/
    return;
}

int VERHOGEN(semd_t *sema4){
    sema4->s_semAdd ++;
    if (sema4->s_semAdd <= 0){
        pcb_PTR temp = removeBlocked(sema4->s_semAdd);
        insertProcQ(readyQ, temp);
    }
    LDST(currentP);
    /*what to return here*/
    return ;
}

int WAITIO(){
    /*value 5 in a0
    the interrupt line number in a1 ([3. . .7])
    the device number in a2 ([0. . .7])
    TRUE or FALSE in a3 to indicate if waiting for a terminal read operation */


    return;
}

int GETCPUTIME(){
    /*the accumulated processor time (in microseconds) used by the requesting process be placed/returned in the caller’s v0*/
    currentP->p_s.s_v0 = currentP->p_time + getTIMER();
}

int WAITCLOCK(){
    /*
    This service performs a P operation on the Pseudo-clock semaphore.
    This semaphore is V’ed every 100 milliseconds by the Nucleus.
    block the Current Process on the ASL then Scheduler is called.*/
    return;
}

int GETSUPPORTPTR(){

    return;
}


int SYSCALL(int syscall,state_t *statep, support_t * supportp, int arg3) {

    return;

}
