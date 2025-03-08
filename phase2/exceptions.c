/*********************************EXCEPTIONS.C*******************************
 *
 * 
 *      Modified by Phuong and Oghap on Feb 2025
 */
#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"

#include "scheduler.h"
#include "interrupts.h"
#include "initial.h"

#include "exceptions.h"

#define pseudo_clock_idx    48

void helper_PASSEREN(int *sema4){
    /*  
         
    */

    (*sema4) --;
    if ((*sema4) < 0){
        insertBlocked(sema4, currentP);
    }
    return;
}

HIDDEN void deep_copy_state_t(state_PTR dest, state_PTR src) {
    /*
    the function that takes in pointer to 2 state_t and deep copy the value src to dest
    */
    dest->s_cause = src->s_cause;
    dest->s_entryHI = src->s_entryHI;
    dest->s_pc = src->s_pc;
    int i;
    for (i = 0; i < STATEREGNUM; i ++){
        dest->s_reg[i] = src->s_reg[i];
    }
    dest->s_status = src->s_status;
}

void deep_copy_context_t(context_t *dest,context_t *src) {
    /*
    the funtion that takes in pointer to 2 support_t and deep copy the value src to dest
    */
    dest->c_pc = src->c_pc;
    dest->c_stackPtr = src->c_stackPtr;
    dest->c_status = src->c_status;
}

void blocking_syscall_handler (){
    /*
    This function handle the steps after a blocking handler including:
    */
    ((state_PTR) BIOSDATAPAGE)->s_pc += 0x4;
    /* save processor state copy into current process pcb*/
    deep_copy_state_t(&(currentP->p_s), BIOSDATAPAGE);
    /*update the cpu time for the current process*/
    currentP->p_time += (5000 - getTIMER());
    /*process was already added to ASL in the syscall =>already blocked*/
    scheduler();
}

void non_blocking_syscall_handler (){
    /*increment PC by 4*/
    ((state_PTR) BIOSDATAPAGE)->s_pc += 0x4;
    /* update the cpu_time*/
    currentP->p_time += (5000 - getTIMER());
    /*save processor state into the "well known" location for return*/
    LDST((state_PTR) BIOSDATAPAGE);
}


int check_KU_mode_bit() {
    /*
    Examines the Status register in the saved exception state. In particular, examine the previous version of the KU bit (KUp)
    */
    int KUp = ((state_PTR) BIOSDATAPAGE)->s_status & 0x00000008;
    if (KUp == 0){
        return 0;
    }
    return 1;
}

void helper_terminate_process(pcb_PTR toBeTerminate){
    pcb_PTR childToBeTerminate;
    /* recursively, all progeny of this process are terminated as well. */
    while (!emptyChild(toBeTerminate)) {
        childToBeTerminate = removeChild(currentP);
        helper_terminate_process(childToBeTerminate);
    }
    /* make the child no longer child of its parent*/
    outChild(toBeTerminate);
    pcb_PTR process_unblocked;
    /*if terminated process is not blocked on our device semaphore */
    if (outBlocked(toBeTerminate) == NULL){
        process_unblocked = outProcQ(&(((semd_t *) toBeTerminate->p_semAdd)->s_procQ), toBeTerminate);
        if (process_unblocked != NULL){
            if ((*toBeTerminate->p_semAdd) < 0){
                (*toBeTerminate->p_semAdd)++;
            }
            softBlock_count --;
        }
    } else {
        softBlock_count --;
    }
    /* if this pcb is in readyQ, take it out*/
    outProcQ(&readyQ, toBeTerminate);
    /* free the pcb and decrease process count*/
    freePcb(toBeTerminate);
    process_count--;
    return;
}

/* Is there a time when we need to determine whether the current process is executing in kernel mode or 
user-mode? */

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
void CREATEPROCESS(){

    pcb_PTR newProcess = allocPcb();

    /* If no more free pcb’s return -1*/
    if (newProcess == NULL) {
        /* return an error code of -1 is placed/returned in the caller’s v0 */
        ((state_PTR) BIOSDATAPAGE)->s_v0 = -1;
        return;
    }

    /* deep copy the process state where a1 contain a pointer to a processor state (state t) */
    deep_copy_state_t(&newProcess->p_s, ((state_PTR) BIOSDATAPAGE)->s_a1);    

    /* deep copy the support struct */
    /* If no parameter is provided, this field is set to NULL. */
    if (((state_PTR) BIOSDATAPAGE)->s_a2 != NULL & ((state_PTR) BIOSDATAPAGE)->s_a2 != 0){
        newProcess->p_supportStruct = ((state_PTR) BIOSDATAPAGE)->s_a2;
    }
    else{
        newProcess->p_supportStruct = NULL;
    }

    /*  
    The process queue fields (e.g. p next) by the call to insertProcQ
•   The process tree fields (e.g. p child) by the call to insertChild. 
    */
    insertProcQ(&readyQ, newProcess);
    insertChild(currentP, newProcess);

    /* return the value 0 in the caller’s v0 */
    ((state_PTR) BIOSDATAPAGE)->s_v0 = 0;

    /* increase process count*/
    process_count++;

    /* p_time is set to 0 and p_semAdd is set to NULL in allocPcb() */
}

void TERMINATEPROCESS(){
    /* recursively terminate child and free pcb */
    helper_terminate_process(currentP);
    /* call scheduler */
    scheduler();
}

void PASSEREN(){
    /*  
        Depending on the value of the semaphore, control is either returned to the
        Current Process, or this process is blocked on the ASL (transitions from “running”
        to “blocked”) and the Scheduler is called.
    */

    /* getting the sema4 address from register a1 */
    int *sema4 = ((state_PTR) BIOSDATAPAGE)->s_a1;

    (*sema4)--;

    if ((*sema4) < 0){
        insertBlocked(sema4, currentP);
        /* executing process is blocked on the ASL and Scheduler is called*/
        blocking_syscall_handler();
    }
    /* control is returned to the Current Process */
    non_blocking_syscall_handler();
    return;
}

pcb_PTR VERHOGEN(){
    /*getting the sema4 address from register a1*/
    int *sema4 = ((state_PTR) BIOSDATAPAGE)->s_a1;

    pcb_PTR process_unblocked;
    (*sema4) ++;
    
    if ((*sema4) <= 0){
        process_unblocked = removeBlocked(sema4);
        if (process_unblocked == NULL){
            return NULL;
        }
        insertProcQ(&readyQ, process_unblocked);
        return process_unblocked;
    }
    return NULL;
}

void WAITIO(){
    /*value 5 in a0
    the interrupt line number in a1 ([3. . .7])
    the device number in a2 ([0. . .7])
    TRUE or FALSE in a3 to indicate if waiting for a terminal read operation */

    /*
    current process from running to blocked -- increase softblockcount, what else
    P operation on the semaphore of that device -- found by a1: interrupt line number + a2: device number + a3: TRUE/FALSE if waiting for a terminal read operation
    block the current process on the asl
    V operation on the semaphore when device generate interupt
    process resume => place device status code in v0 (char received or transmitted?)
    */
   
    /* must also update the Cause.IP field bits to show which interrupt lines are pending -- no, the hardware do this*/

    int device_idx = devSemIdx(((state_PTR) BIOSDATAPAGE)->s_a1, ((state_PTR) BIOSDATAPAGE)->s_a2,  ((state_PTR) BIOSDATAPAGE)->s_a2);

    helper_PASSEREN(&(device_sem[device_idx]));

    softBlock_count ++;
    
    /*returning the device status word*/
    ((state_PTR) BIOSDATAPAGE)->s_v0 = devAddrBase(((state_PTR) BIOSDATAPAGE)->s_a1, ((state_PTR) BIOSDATAPAGE)->s_a2);
    /* 
    blocking_syscall_handler();
    */
    return;
}

void GETCPUTIME(){
    /*the accumulated processor time (in microseconds) used by the requesting process be placed/returned in the caller’s v0*/
    ((state_PTR) BIOSDATAPAGE)->s_v0 = currentP->p_time + 5000 - getTIMER();
    return;
}

void WAITCLOCK(){
    /*
    This service performs a P operation on the Pseudo-clock semaphore.
    This semaphore is V’ed every 100 milliseconds by the Nucleus.
    block the Current Process on the ASL then Scheduler is called.*/
    
    helper_PASSEREN(&(device_sem[pseudo_clock_idx]));

    softBlock_count ++;

    /*scheuler is called in SYSCALL handler*/
    /* 
    blocking_syscall_handler();
    */
    return;
}

void GETSUPPORTPTR(){
    /*
    returns the value of p supportStruct from the Current Process’s pcb
    */
    ((state_PTR) BIOSDATAPAGE)->s_v0 = currentP->p_supportStruct;
}

/* The Nucleus Program Trap exception handler performs a standard Pass Up or Die operation 
using the GENERALEXCEPT index value. */
/* 
1. copy the saved exception state into a location accessible to the Support Level, 
2. and pass control to a routine specified by the Support Level.
*/
void pass_up_or_die(int exception_constant) {

   /* If the Current Process’s p supportStruct is NULL, 
    then the exception should be handled as a SYS2: the Current Process and all its progeny are terminated. */
    if (currentP -> p_supportStruct == NULL){
        helper_terminate_process(currentP);
    }else{
        /* Copy the saved exception state from the BIOS Data Page to the correct sup exceptState field of the Current Process. 
        Perform a LDCXT using the fields from the correct sup exceptContextfield of the Current Process. */
        deep_copy_state_t(&currentP -> p_supportStruct->sup_exceptState[exception_constant], BIOSDATAPAGE);
        LDCXT(currentP -> p_supportStruct -> sup_exceptContext[exception_constant].c_stackPtr, 
            currentP -> p_supportStruct -> sup_exceptContext[exception_constant].c_status, 
            currentP -> p_supportStruct -> sup_exceptContext[exception_constant].c_pc);
        /* NOTE: How did we have the context in the sup_exceptContext in the supportStruct of the current process? */
    }
}


void SYSCALL_handler() {
    /*int syscall,state_t *statep, support_t * supportp, int arg3*/
    /*check if in kernel mode -- if not, put 10 for RI into exec code field in cause register and call program trap exception*/
    if (check_KU_mode_bit() != 0){
        ((state_PTR) BIOSDATAPAGE)->s_cause = ((state_PTR) BIOSDATAPAGE)->s_cause | 0x00000028;
        ((state_PTR) BIOSDATAPAGE)->s_cause = ((state_PTR) BIOSDATAPAGE)->s_cause & 0xFFFFFFEB;

        /* Program Traps */
        pass_up_or_die(GENERALEXCEPT);
        return;
    }
    
    switch (((state_PTR) BIOSDATAPAGE)->s_a0) {
    case 1:
        CREATEPROCESS();
        break;
    case 2:
        TERMINATEPROCESS();    
        break;
    case 3:
        PASSEREN();
        break;
    case 4:
        VERHOGEN();
        break;
    case 5:
        WAITIO();
        break;
    case 6:
        GETCPUTIME();
        break;
    case 7:
        WAITCLOCK();
        break;
    case 8:
        GETSUPPORTPTR();
        break;
    default:
        /* Syscall Exception Error - Program trap handler */
        pass_up_or_die(GENERALEXCEPT);
    }

    /*increment PC by 4*/
    ((state_PTR) BIOSDATAPAGE)->s_pc += 0x4;

    /* update the cpu_time*/
    currentP->p_time += (5000 - getTIMER());
    /*if the syscall was blocking*/
    if ((((state_PTR) BIOSDATAPAGE)->s_a0 == 3 ) || (((state_PTR) BIOSDATAPAGE)->s_a0 == 5) || (((state_PTR) BIOSDATAPAGE)->s_a0 == 7)){
        /* save processor state copy into current process pcb*/
        deep_copy_state_t(&(currentP->p_s), BIOSDATAPAGE);
        /*process was already added to ASL in the syscall => already blocked*/
        scheduler();
    }
    /*save processor state into the "well known" location for nonblocking syscall*/
    LDST(((state_PTR) BIOSDATAPAGE));

}

void exception_handler(){
    /* Get the Cause registers from the saved exception state and 
    use AND bitwise operation to get the .ExcCode field */
    /* decodes Cause.ExcCode */
    int ExcCode = CauseExcCode(((state_PTR) BIOSDATAPAGE)->s_cause);

    switch (ExcCode)
    {
    case INT:
        /* external device interrupt */
        interrupt_exception_handler();
        break;
    case MOD:
    case TLBL:
    case TLBS:
        pass_up_or_die(PGFAULTEXCEPT);
        break;
    case ADEL:
    case ADES:
    case IBE:
    case DEB:
    case BP:
    case RI:
    case CPU:
    case OV:
        pass_up_or_die(GENERALEXCEPT);
        break;
    case SYS:
        SYSCALL_handler();
        break;
    default:
        break;
    }
}
