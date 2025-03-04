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
#include "exceptions.h"

#define pseudo_clock_idx    48

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

    /* If no more free pcb’s, then ... */
    if (newProcess == NULL) {
        /* return an error code of -1 is placed/returned in the caller’s v0 */
        currentP->p_s.s_v0 = -1;
    }

    /* deep copy the process state where a1 contain a pointer to a processor state (state t) */
    deep_copy_state_t(&newProcess->p_s, currentP->p_s.s_a1);    

    /* deep copy the support struct */
    /* If no parameter is provided, this field is set to NULL. */
    if (currentP->p_s.s_a2 != NULL | currentP->p_s.s_a2 == 0){
        newProcess->p_supportStruct = currentP->p_s.s_a2;
    }else{
        newProcess->p_supportStruct = NULL;
    }

    /*  
    The process queue fields (e.g. p next) by the call to insertProcQ
•   The process tree fields (e.g. p child) by the call to insertChild. 
    */
    insertProcQ(readyQ, newProcess);
    insertChild(currentP, newProcess);

    /* return the value 0 in the caller’s v0 */
    currentP->p_s.s_v0 = 0;
    process_count++;
}

void TERMINATEPROCESS(){
    /* recursively terminate child and free pcb */
    helper_terminate_process(currentP);
}

void PASSEREN(){ //use and update a1
    /*  
        Depending on the value of the semaphore, control is either returned to the
        Current Process, or this process is blocked on the ASL (transitions from “running”
        to “blocked”) and the Scheduler is called.
    */

    /* getting the sema4 address from register a1 */
    semd_t *sema4 = currentP->p_s.s_a1;

    *(sema4->s_semAdd) = *(sema4->s_semAdd) --;
    if (*(sema4->s_semAdd) < 0){
        insertBlocked(sema4->s_semAdd, currentP);
        /*should or should not call scheudler here? if call scheduler here, also need to increament pc here?-- decided to do it in syscall hander at the end may change later by uncomment calling blocking_syscall_handler right below and comment and comment things at in syscall handler way below*/
        /*
        blocking_syscall_handler();
        */
    }
    return;
}

pcb_PTR VERHOGEN(){
    /*getting the sema4 address from register a1*/
    semd_t *sema4 = currentP->p_s.s_a1;
    pcb_PTR temp;
    *(sema4->s_semAdd) = *(sema4->s_semAdd) ++;
    if (*(sema4->s_semAdd) <= 0){
        temp = removeBlocked(sema4->s_semAdd);
        insertProcQ(readyQ, temp);
    }
    return temp;
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

    /*terminal transmission higher priority than terminal receipt*/
    int device_idx = devSemIdx(currentP->p_s.s_a1, currentP->p_s.s_a2,  currentP->p_s.s_a2); /*does the pseudoclock generate interupt using this too? No right? Cause this is just interrupt line 3 to 7 but clock and stuff use other line (PLT use line 1)*/
    
    /*is it correct to put the address of sema4 into a1 like this?*/
    helper_block_currentP(&(device_sem[device_idx]));
    
    /*put the device sema4 address into register a1 to call P*/
    currentP->p_s.s_a1 = &(device_sem[device_idx]);

    PASSEREN();

    
    
    /*returning the device status word*/
    currentP->p_s.s_v0 = devAddrBase(currentP->p_s.s_a1, currentP->p_s.s_a2);
    /* 
    blocking_syscall_handler();
    */
    return;
}

void GETCPUTIME(){
    /*the accumulated processor time (in microseconds) used by the requesting process be placed/returned in the caller’s v0*/
    int interval_current;
    STCK(interval_current);
    currentP->p_s.s_v0 = currentP->p_time + (interval_current - interval_start);
    return;
}

void WAITCLOCK(){
    /*
    This service performs a P operation on the Pseudo-clock semaphore.
    This semaphore is V’ed every 100 milliseconds by the Nucleus.
    block the Current Process on the ASL then Scheduler is called.*/

    helper_block_currentP(device_sem[pseudo_clock_idx]);

    /*put the pseudoclock sema4 into the register a1 to do P operation*/
    currentP->p_s.s_a1 = device_sem[pseudo_clock_idx];
    
    PASSEREN();

    /*scheuler is called in SYSCALL handler*/
    blocking_syscall_handler();
    return;
}

void GETSUPPORTPTR(){
    /*
    returns the value of p supportStruct from the Current Process’s pcb
    */
    currentP->p_s.s_v0 = currentP->p_supportStruct;
}

void helper_block_currentP(int *semdAdd){
    /* 
    Helper function that block the current process and place on the ASL and increase softblock count
    parameter: the (device) sema4
    */
    softBlock_count += 1;

    /*insert the process into ASL*/
    insertBlocked(semdAdd, currentP);
}


pcb_PTR helper_unblock_process(int *semdAdd){
    /*
    Helper function that unblock 1 process and decrease softblock count
    */
    /* remove the process from ASL*/
    pcb_PTR unblocked_pcb = removeBlocked(semdAdd);

    if (unblocked_pcb != NULL){
        softBlock_count -= 1;
    }
    return unblocked_pcb;
}

void helper_terminate_process(pcb_PTR toBeTerminate){
    /*are we terminating the current process, then where are we supposed to put the return value in - a2 of currentP ???*/

    /* recursively, all progeny of this process are terminated as well. */
    while (!emptyChild(toBeTerminate)) {
        pcb_PTR childToBeTerminate = removeChild(currentP);
        TERMINATEPROCESS(childToBeTerminate);
    }
    outChild(toBeTerminate);
    freePcb(toBeTerminate);
    process_count--;
    return;
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
        TERMINATEPROCESS();
    }else{
        /* Copy the saved exception state from the BIOS Data Page to the correct sup exceptState field of the Current Process. 
        Perform a LDCXT using the fields from the correct sup exceptContextfield of the Current Process. */
        deep_copy_state_t(&currentP -> p_supportStruct->sup_exceptState[exception_constant], BIOSDATAPAGE);
        LDCXT(currentP -> p_supportStruct -> sup_exceptContext->c_stackPtr,
            currentP -> p_supportStruct -> sup_exceptContext->c_status, 
            currentP -> p_supportStruct -> sup_exceptContext->c_pc);
    }
    /* NOTE: How did we have the context in the sup_exceptContext in the supportStruct of the current process? */
}

int check_KU_mode_bit() {
    /*
    Examines the Status register in the saved exception state. In particular, examine the previous version of the KU bit (KUp)
    */
    int KUp = currentP->p_s.s_status & 0x00000008;
    return 0;
}

void deep_copy_state_t(state_PTR dest, state_PTR src) {
    /*
    the funtion that takes in pointer to 2 state_t and deep copy the value src to dest
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

void deep_copy_support_t(support_t *dest,support_t *src) {
    /*
    the funtion that takes in pointer to 2 support_t and deep copy the value src to dest
    */
    dest->sup_asid = src->sup_asid;
    deep_copy_context_t(dest->sup_exceptContext, &src->sup_exceptContext);
    int i;
    for (i = 0; i < STATEREGNUM; i ++){
        deep_copy_state_t(&dest->sup_exceptState[i], &src->sup_exceptState[i]);
    }
}

void deep_copy_context_t(context_t *dest,context_t *src) {
    /*
    the funtion that takes in pointer to 2 support_t and deep copy the value src to dest
    */
    dest->c_pc = src->c_pc;
    dest->c_stackPtr = src->c_stackPtr;
    dest->c_status = src->c_status;
}

unsigned int SYSCALL_handler() {
    /*int syscall,state_t *statep, support_t * supportp, int arg3*/
    /*check if in kernel mode -- if not, put 10 for RI into exec code field in cause register and call program trap exception*/
    if (check_KU_mode() != 0){
        /* clear out current exec code bit field in cause registers*/
        currentP->p_s.s_cause = currentP->p_s.s_cause & (~EXECCODEBITS);
        /* put RI value into the exec code bit field which is from 2 to 6 so we shift RI by 2 and do OR operation*/
        currentP->p_s.s_cause = currentP->p_s.s_cause | (RI << 2);
        /* save the new cause register*/
        ((state_t *) BIOSDATAPAGE)->s_cause = currentP->p_s.s_cause;

        /* Program Traps */
        pass_up_or_die(GENERALEXCEPT);
        return 1;
    }

   switch (currentP->p_s.s_a0) {
    case 1:
        CREATEPROCESS();
        break;
    case 2:
        TERMINATEPROCESS();    
        return;
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
        /* Syscall Exception Error ? Program trap handler?*/
        pass_up_or_die(GENERALEXCEPT);
    }

    /*increment PC by 4*/
    currentP->p_s.s_pc += 0x4;
    /* update the cpu_time*/
    currentP->p_time += (5000 - getTIMER());
    
    /*if the syscall was blocking*/
    if (currentP->p_s.s_a0 == 3 | currentP->p_s.s_a0 == 5 | currentP->p_s.s_a0 == 7){
        /*process was already added to ASL in the syscall =>already blocked*/
        scheduler();
    }
    
    /*save processor state into the "well known" location for nonblocking syscall*/
    deep_copy_state_t(BIOSDATAPAGE, &currentP->p_s);
    LDST(&(currentP->p_s));

}

void blocking_syscall_handler (){
    /*
    This function handle the steps after a blocking handler including:
    */
    currentP->p_s.s_pc += 0x4;
    /*already copied the saved processor state into the current process pcb at the beginning of SYSCALL_handler as that what we have been using*/
    /*update the cpu time for the current process*/
    currentP->p_time += (5000 - getTIMER());
    /*process was already added to ASL in the syscall =>already blocked*/
    scheduler();
}

void non_blocking_syscall_handler (){
    /*increment PC by 4*/
    currentP->p_s.s_pc += 0x4;
    /* update the cpu_time*/
    currentP->p_time += (5000 - getTIMER());
    /*save processor state into the "well known" location for return*/
    deep_copy_state_t(BIOSDATAPAGE, &currentP->p_s);
    LDST(&(currentP->p_s));
}