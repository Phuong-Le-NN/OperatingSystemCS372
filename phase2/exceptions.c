/*********************************EXCEPTIONS.C*******************************
 *  Implementation of the Exception Handling Module
 *  
 *  Responsible for handling all exceptions that occur in the system:
 *  -  Interrupts : External device signals.
 *  -  System Calls (SYSCALLs) : Requests for system services.
 *  -  TLB Exceptions : Memory access violations.
 *  -  Program Traps : Invalid operations.
 *  
 *  This module includes the following functions:
 *  - exception_handler(): Determines the type of exception and
 *    delegates processing to specialized handlers.
 *  - interrupt_exception_handler(): Handles external device interrupts
 *  - SYSCALL_handler(): Processes system calls (SYS1–SYS8), allowing user processes to
 *    request services such as process management, I/O operations, and clock waiting.
 *  - pass_up_or_die(): Handles program traps and TLB exceptions. If the process
 *    has a support structure, the exception is passed up to the user-level handler;
 *    otherwise, the process and its children are terminated.
 *  
 *  The exception handling system integrates with the process scheduler to manage
 *  blocked.
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

void debugException(int a0){

}

/* Helper Functions*/

/**********************************************************
 *  helper_PASSEREN()
 *
 *  This function performs a P operation on the given semaphore.
 *  If the semaphore value is negative after decrementing, the
 *  calling process is blocked.
 *
 *  Parameters:
 *         int *sema4 - Pointer to the semaphore to decrement
 *
 *  Returns:
 *         
 **********************************************************/
HIDDEN void helper_PASSEREN(int *sema4){

    (*sema4) --;
    if ((*sema4) < 0){
        insertBlocked(sema4, currentP);
    }
    return;
}

/**********************************************************
 *  deep_copy_state_t()
 *
 *  Deep copies the contents of a source processor state into a
 *  destination processor state.
 *
 *  Parameters:
 *         state_PTR dest - Pointer to the destination state
 *         state_PTR src  - Pointer to the source state
 *
 *  Returns:
 *         
 **********************************************************/
HIDDEN void deep_copy_state_t(state_PTR dest, state_PTR src) {
    dest->s_cause = src->s_cause;
    dest->s_entryHI = src->s_entryHI;
    dest->s_pc = src->s_pc;
    int i;
    for (i = 0; i < STATEREGNUM; i ++){
        dest->s_reg[i] = src->s_reg[i];
    }
    dest->s_status = src->s_status;
}

/**********************************************************
 *  helper_blocking_syscall_handler()
 *
 *  Handles blocking system calls by:
 *  - Incrementing the saved program counter
 *  - Copies the current process state from BIOS
 *  - Updating CPU time
 *  - Calling the scheduler
 *
 *  Parameters:
 *
 *  Returns:
 *         
 **********************************************************/
HIDDEN void helper_blocking_syscall_handler (){
    /*
    This function handle the steps after a blocking handler including:
    */
    ((state_PTR) BIOSDATAPAGE)->s_pc += WORDLEN;
    /* save processor state copy into current process pcb*/
    deep_copy_state_t(&(currentP->p_s), BIOSDATAPAGE);
    /*update the cpu time for the current process*/
    currentP->p_time += (5000 - getTIMER());
    /*process was already added to ASL in the syscall =>already blocked*/
    scheduler();
}


/**********************************************************
 *   helper_non_blocking_syscall_handler()
 * 
 *  Handles non-blocking system calls by:
 *  - Incrementing the saved program counter
 *  - Updating CPU time
 *  - Load the process state into BIOS
 *
 *  Parameters:
 *
 *  Returns:
 * 
 **********************************************************/
HIDDEN void  helper_non_blocking_syscall_handler (){
    /*increment PC by 4*/
    ((state_PTR) BIOSDATAPAGE)->s_pc += WORDLEN;
    /* update the cpu_time*/
    currentP->p_time += (5000 - getTIMER());
    /*save processor state into the "well known" location for return*/
    LDST((state_PTR) BIOSDATAPAGE);
}

/**********************************************************
 *  check_KU_mode_bit()
 * 
 *  Determines whether the current process is in Kernel mode
 *  by examining the KU bit in the saved status register.
 *
 *  Parameters:
 *
 *  Returns:
 *         int - 0 if Kernel mode, 1 if User mode
 **********************************************************/
HIDDEN int check_KU_mode_bit() {
    /*
    Examines the Status register in the saved exception state. 
    In particular, examine the previous version of the KU bit (KUp)
    */
    int KUp = ((state_PTR) BIOSDATAPAGE)->s_status & 0x00000008;
    if (KUp == 0){
        return 0;
    }
    return 1;
}


/**********************************************************
 *  helper_terminate_process()
 *
 *  Recursively terminates a process and all its children processes.
 *  Removes the process from the process queue, device semaphores,
 *  and releases.
 *
 *  Parameters:
 *         pcb_PTR toBeTerminate - Pointer to the process to terminate
 *
 *  Returns:
 *         
 **********************************************************/
HIDDEN void helper_terminate_process(pcb_PTR toBeTerminate){
    pcb_PTR childToBeTerminate;
    /* recursively, all progeny of this process are terminated as well. */
    while (!emptyChild(toBeTerminate)) {
        childToBeTerminate = removeChild(toBeTerminate);
        helper_terminate_process(childToBeTerminate);
    }
    /* make the child no longer child of its parent*/
    outChild(toBeTerminate);
    pcb_PTR process_unblocked;
    /*if terminated process is not blocked on our device semaphore */
    if (toBeTerminate->p_semAdd < &device_sem[0] ||
        toBeTerminate->p_semAdd > &device_sem[DEVINTNUM * DEVPERINT + DEVPERINT + 1 - 1]){
        process_unblocked = outBlocked(toBeTerminate);
        if (process_unblocked != NULL){
            if ((*toBeTerminate->p_semAdd) < 0){
                (*toBeTerminate->p_semAdd)++;
            }
        }
    } else {
        process_unblocked = outBlocked(toBeTerminate);
        if (process_unblocked != NULL){
            softBlock_count --;
        }
    }
    /* if this pcb is in readyQ, take it out*/
    outProcQ(&readyQ, toBeTerminate);
    /* free the pcb and decrease process count*/
    freePcb(toBeTerminate);
    process_count--;
    return;
}


/* System Calls Functions*/


/**********************************************************
 *  CREATEPROCESS()
 *
 *  Allocates and initializes a new process, setting up its
 *  state and inserting it into the ready queue. The new process
 *  becomes a child of the currently running process.
 *
 *  Parameters:
 *
 *  Returns:
 * 
 **********************************************************/
HIDDEN void CREATEPROCESS(){

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


/**********************************************************
 *  TERMINATEPROCESS() 
 *
 *  Terminates the currently running process and all its
 *  child processes. The scheduler is called after termination.
 *
 *  Parameters:
 *         
 *
 *  Returns:
 * 
 **********************************************************/
HIDDEN void TERMINATEPROCESS(){
    /* recursively terminate child and free pcb */
    helper_terminate_process(currentP);
    /* call scheduler */
    scheduler();
}

/**********************************************************
 *  PASSEREN()
 *
 *  Performs a P operation on the semaphore. If the
 *  semaphore value is negative, the calling process is blocked.
 *
 *  Parameters:
 *        
 *
 *  Returns:
 *         
 **********************************************************/
HIDDEN void PASSEREN(){
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
        helper_blocking_syscall_handler();
    }
    /* control is returned to the Current Process */
    helper_non_blocking_syscall_handler();
    return;
}

/**********************************************************
 *  VERHOGEN() 
 *
 *  Performs a V operation on the semaphore. If a process
 *  is blocked on the semaphore, it is unblocked and moved to
 *  the ready queue.
 *
 *  Parameters:
 *
 *  Returns:
 *         pcb_PTR - Pointer to the unblocked process 
 **********************************************************/
HIDDEN pcb_PTR VERHOGEN(){
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

/**********************************************************
 *  WAITIO()
 *
 *  Blocks the current process until the requested I/O operation
 *  completes. The process is added to the appropriate device
 *  semaphore queue.
 *
 *  Parameters:
 *
 *  Returns:
 * 
 **********************************************************/
HIDDEN void WAITIO(){
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
    int device_idx = devSemIdx(((state_PTR) BIOSDATAPAGE)->s_a1, ((state_PTR) BIOSDATAPAGE)->s_a2,  ((state_PTR) BIOSDATAPAGE)->s_a3);

    helper_PASSEREN(&(device_sem[device_idx]));

    softBlock_count ++;
    
    return;
}

/**********************************************************
 *  GETCPUTIME()
 *
 *  Returns the total CPU time used by the calling process.
 *
 *  Parameters:
 *         
 *
 *  Returns:
 *         
 **********************************************************/

HIDDEN void GETCPUTIME(){

    /*the accumulated processor time (in microseconds) used by the requesting 
    process be placed/returned in the caller’s v0*/
    ((state_PTR) BIOSDATAPAGE)->s_v0 = currentP->p_time + 5000 - getTIMER();
    return;
}


/**********************************************************
 *  WAITCLOCK()
 *
 *  Performs a P operation on the pseudo-clock semaphore. The
 *  process is blocked until the next clock tick (every 100ms).
 * 
 *  This service performs a P operation on the Pseudo-clock semaphore.
 *  This semaphore is V’ed every 100 milliseconds by the Nucleus.
 *  Block the Current Process on the ASL then Scheduler is called.
 *
 *  Parameters:
 *         
 *
 *  Returns:
 *        
 **********************************************************/
HIDDEN void WAITCLOCK(){
    
    helper_PASSEREN(&(device_sem[pseudo_clock_idx]));

    softBlock_count ++;

    return;
}


/**********************************************************
 *  GETSUPPORTPTR()
 *
 *  Returns the value of p_supportStruct from the Current 
 *  Process’s pcb
 *
 *  Parameters:
 *         None
 *
 *  Returns:
 *         
 **********************************************************/
HIDDEN void GETSUPPORTPTR(){

    ((state_PTR) BIOSDATAPAGE)->s_v0 = currentP->p_supportStruct;
}


/**********************************************************
 *  pass_up_or_die()
 *
 *  Handles program traps and TLB exceptions by either passing
 *  them to the user-level handler (if present) or terminating
 *  the process.
 *
 *  Parameters:
 *         int exception_constant - Exception type
 *
 *  Returns:
 *         
 **********************************************************/
HIDDEN void pass_up_or_die(int exception_constant) {

    /* If the Current Process’s p supportStruct is NULL, 
    then the exception should be handled as a SYS2: the Current Process and all its progeny are terminated. */
    if (currentP -> p_supportStruct == NULL){
        helper_terminate_process(currentP);
        scheduler();
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


/**********************************************************
 *  SYSCALL_handler()
 *
 *  Decodes and processes system calls (SYS1–SYS8). If an
 *  invalid system call is encountered, a program trap is 
 *  triggered.
 *
 *  Parameters:
 *         
 *
 *  Returns:
 *         
 **********************************************************/
HIDDEN void SYSCALL_handler() {
    /*int syscall,state_t *statep, support_t * supportp, int arg3*/
    /*check if in kernel mode -- if not and not SYSCALL 9+ either, put 10 for RI into exec code field in cause register and call program trap exception*/
    if (check_KU_mode_bit() != 0){
        if (((state_PTR) BIOSDATAPAGE)->s_a0 <= 8){
            debugException(currentP->p_supportStruct->sup_asid);
            ((state_PTR) BIOSDATAPAGE)->s_cause = ((state_PTR) BIOSDATAPAGE)->s_cause | 0x00000028;
            ((state_PTR) BIOSDATAPAGE)->s_cause = ((state_PTR) BIOSDATAPAGE)->s_cause & 0xFFFFFFEB;
        }
        
        /* Program Traps */
        pass_up_or_die(GENERALEXCEPT);
        return;
    }
    
    switch (((state_PTR) BIOSDATAPAGE)->s_a0) {
        case 1:
            CREATEPROCESS();
             helper_non_blocking_syscall_handler();
        case 2:
            TERMINATEPROCESS();    
            break;
        case 3:
            PASSEREN();
            break;
        case 4:
            VERHOGEN();
             helper_non_blocking_syscall_handler();
        case 5:
            WAITIO();
            helper_blocking_syscall_handler ();
        case 6:
            GETCPUTIME();
             helper_non_blocking_syscall_handler();
        case 7:
            WAITCLOCK();
            helper_blocking_syscall_handler ();
        case 8:
            GETSUPPORTPTR();
             helper_non_blocking_syscall_handler();
        default:
            /* Syscall Exception Error - Program trap handler */
            pass_up_or_die(GENERALEXCEPT);
    }
}

/**********************************************************
 *  exception_handler()
 *
 *  Determines the cause of an exception and delegates it to
 *  the corresponding handler (interrupt, TLB exception, program
 *  trap, or system call).
 *
 *  Parameters:
 *         
 *
 *  Returns:
 *         
 **********************************************************/
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
