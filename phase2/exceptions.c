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

#define pseudo_clock_idx    48

/*is declaring this a global bad?*/
HIDDEN pcb_PTR callerProc = BIOSDATAPAGE;                    

/*
    The process requesting the SYS1 service continues to exist and to execute.
    If the new process cannot be created due to lack of resources (e.g. no more free
    pcb’s), an error code of -1 is placed/returned in the caller’s v0, otherwise, return
    the value 0 in the caller’s v0.
*/
void CREATEPROCESS(){

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
        callerProc->p_s.s_v0 = -1;
        return -1;
    }
    newProcess->p_s = *((state_PTR) (callerProc->p_s.s_a1));
    newProcess->p_supportStruct = ((state_PTR) (callerProc->p_s.s_a2));
    insertProcQ(readyQ, newProcess);
    insertChild(callerProc, newProcess);
    newProcess->p_time = 0;
    newProcess->p_semAdd = NULL;

    /* how can we check if there is no parameter provided */
    callerProc->p_s.s_v0 = 0;
    return 0;
}

void TERMINATEPROCESS(){
    helper_terminate_process(callerProc);
}

void PASSEREN(){ //use and update a1
    /*  
        Depending on the value of the semaphore, control is either returned to the
        Current Process, or this process is blocked on the ASL (transitions from “running”
        to “blocked”) and the Scheduler is called.
    */    
    /*getting the sema4 address from register a1*/
    semd_t *sema4 = callerProc->p_s.s_a1;

    sema4->s_semAdd --;
    if (sema4->s_semAdd < 0){
        insertBlocked(sema4->s_semAdd, callerProc);
        scheduler();
    }
    LDST(callerProc);
    /*what to return here*/
    return;
}

void VERHOGEN(){
    /*getting the sema4 address from register a1*/
    semd_t *sema4 = callerProc->p_s.s_a1;

    sema4->s_semAdd ++;
    if (sema4->s_semAdd <= 0){
        pcb_PTR temp = removeBlocked(sema4->s_semAdd);
        insertProcQ(readyQ, temp);
    }
    LDST(callerProc);
    /*what to return here*/
    return ;
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
    /* must also update the Cause.IP field bits to show which interrupt lines are pending -- no, the hardware do this?*/

    /*terminal transmission higher priority than terminal receipt*/
    int device_idx = (callerProc->p_s.s_a1 - 3) * 8 + callerProc->p_s.s_a3 * 8 + callerProc->p_s.s_a2; /*does the pseudoclock generate interupt using this too? No right? Cause this is just interrupt line 3 to 7 but clock and stuff use other line (PLT use line 1)*/
    
    /*is it correct to put the address of sema4 into a1 like this?*/
    helper_block_callerProc(&device_sem[device_idx]);
    
    /*put the device sema4 address into register a1 to call P*/
    callerProc->p_s.s_a1 = device_sem[device_idx];
    PASSEREN();
    
    scheduler();
    
    /*once the interrupt received <= the device finish, call V on the same device sema4 address*/
    VERHOGEN();
    
    /*returning the device status word*/
    callerProc->p_s.s_v0 = devAddrBase(callerProc->p_s.s_a1, callerProc->p_s.s_a2);
    return;
}

void GETCPUTIME(){
    /*the accumulated processor time (in microseconds) used by the requesting process be placed/returned in the caller’s v0*/

    callerProc->p_s.s_v0 = callerProc->p_time + (5000 - getTIMER()); /*because we load 5000 to PLT when using scheduler to let the process run*/
}

void WAITCLOCK(){
    /*
    This service performs a P operation on the Pseudo-clock semaphore.
    This semaphore is V’ed every 100 milliseconds by the Nucleus.
    block the Current Process on the ASL then Scheduler is called.*/

    helper_block_callerProc(device_sem[pseudo_clock_idx]);

    /*put the pseudoclock sema4 into the register a1 to do P operation*/
    callerProc->p_s.s_a1 = device_sem[pseudo_clock_idx];
    PASSEREN();

    scheduler();

    return;
}

void GETSUPPORTPTR(){
    /*
    returns the value of p supportStruct from the Current Process’s pcb
    */
    callerProc->p_s.s_v0 = callerProc->p_supportStruct;
}

void helper_block_callerProc(int *semdAdd){
    /* 
    Helper function that block the current process and place on the ASL
    parameter: the (device) sema4
    */
    softBlock_count += 1; /*which status bit of the pcb to set if any?*/

    /*insert the process into ASL*/
    insertBlocked(semdAdd, callerProc);
}

void helper_terminate_process(pcb_PTR toBeTerminate){
    /*are we terminating the current process, then where are we supposed to put the return value in - a2 of callerProc ???*/

    /* recursively, all progeny of this process are terminated as well. */
    while (!emptyChild(toBeTerminate)) {
        pcb_PTR childToBeTerminate = removeChild(callerProc);
        TERMINATEPROCESS(childToBeTerminate);
    }
    outChild(toBeTerminate);
    freePcb(toBeTerminate);
    return;
}

void program_trap_exception_handler(){

}

int check_KU_mode_bit() {
    /*
    Examines the Status register in the saved exception state. In particular, examine the previous version of the KU bit (KUp)
    */
}


unsigned int SYSCALL(unsigned int number, unsigned int arg1, unsigned int arg2, unsigned int arg3) {
    /*int syscall,state_t *statep, support_t * supportp, int arg3*/
    /*
    Cause.Exc code set to 8
    set Cause.IP ? there are 8 lines, line 3 to 7 is for peripheral devices -- should we set this in this function or should we set this in SYSCALL5?
    */

    /*check if in kernel mode -- if not call program trap exception*/
    if (check_KU_mode() == 1){
        program_trap_exception_handler();
        return 1;
    }

    /*Load arguments into registers*/
    callerProc->p_s.s_a1 = arg1;
    callerProc->p_s.s_a2 = arg2;
    callerProc->p_s.s_a3 = arg3;

   switch (number) {
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
    }

    return callerProc->p_s.s_v0;

}
