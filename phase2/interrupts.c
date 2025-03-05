/*********************************INTERRUPTS.C*******************************
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
#include "exceptions.h"
#include "initial.h"

#include "interrupts.h"

#define pseudo_clock_idx    48
#define IPBITSPOS       8
#define INTLINESCOUNT   8

/* Helper Functions */

HIDDEN void deep_copy_state_t(state_PTR dest, state_PTR src) {
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

int check_interrupt_line(int idx){
    /*
    The function that takes in a line number and check if there is interrupt pending on that line
    Return TRUE or FALSE
    */
    /* get the IP bit from cause registers then shift right to get the interrupt line which*/
    /* as a hex, on bits indicate line with interrupt pending -- this is Cause.IP*/
    int IPLines = (getCAUSE()&IPBITS) >> IPBITSPOS;
    /* 31 as the size of int is 32 bits (4 bytes)*/
    if ((IPLines << (31 - idx)) >> (31) == 0) {
        return FALSE;
    }
    return TRUE;
}

int check_interrupt_device(int idx, int* devRegAdd){
    /* 
    The fucntion that check if a device has interrupt pending that takes in a device number and address of the Line Interrupt Device Bit Map
    Return TRUE or FALSE
    */
    /* loop through the devices on each line*/
    int devBits = *devRegAdd;
    /* 31 as the size of int is 32 bits (4 bytes)*/
    if ((devBits << (31 - idx)) >> (31) == 0) {
        return FALSE;
    }
    return TRUE;
}

void deep_copy_device_t(device_t *dest, device_t *src){
    dest->d_command = src->d_command;
    dest->d_data0 = src->d_data0;
    dest->d_data1 = src->d_data1;
    dest->d_status = src->d_status;
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

void helper_terminal_read(int intLineNo, int devNo){
    /*
    Helper function to acknowledge interrupts from terminal read sub-device
    for terminal device, data1 is trasm_command, data0 is transm_status, comman is recv_comman, status is recv_status
    */

    /* Calculate the address for this device’s device register */
    device_t *intDevRegAdd = (device_t*) devAddrBase(intLineNo, devNo);

    /* Save off the status code from the device’s device register*/
    int savedDevRegStatus = intDevRegAdd->d_data0;
        
    /* Acknowledge the outstanding interrupt */
    intDevRegAdd->d_data1 = ACK;

    /* Perform a V operation on the Nucleus maintained semaphore associated with this (sub)device.*/
    int devIdx = devSemIdx(intLineNo, devNo, 1);
    /* put semdAdd into currentP state register a1 to call VERHOGEN -- VERHOGEN use the state in currentP*/
    ((state_PTR) BIOSDATAPAGE)->s_a1 = (int) &device_sem[devIdx];
    VERHOGEN();
    /* putting the process returned by V operation to unblocked_pcb */
    pcb_PTR unblocked_pcb = (pcb_PTR) ((state_PTR) BIOSDATAPAGE)->s_v0;

    /* if V operation failed to remove a pcb then return control to the current process*/
    if (unblocked_pcb == NULL){
        /* if there is no current process to return to either, call scheduler*/
        if (currentP == NULL){
            scheduler();
        }
        LDST((void *) BIOSDATAPAGE);
    }

    /* Place the stored off status code in the newly unblocked pcb’s v0 register.*/
    unblocked_pcb->p_s.s_v0 = savedDevRegStatus;

    /* Insert the newly unblocked pcb on the Ready Queue*/
    /* Done in VERHOGEN()*/

    /* Perform a LDST on the saved exception state*/
    LDST((void *) BIOSDATAPAGE);
}

void helper_terminal_write_other_device(int intLineNo, int devNo){
    /*
    Helper function to acknowledge interrupts from terminal write sub-device and other devices
    */

    /* Calculate the address for this device’s device register */
    device_t *intDevRegAdd = (device_t*) devAddrBase(intLineNo, devNo);

    /* Save off the status code from the device’s device register*/
    int savedDevRegStatus = intDevRegAdd->d_status;
        
    /* Acknowledge the outstanding interrupt */
    intDevRegAdd->d_command = ACK;

    /* Perform a V operation on the Nucleus maintained semaphore associated with this (sub)device.*/
    int devIdx = devSemIdx(intLineNo, devNo, 0);
    /* put semdAdd into currentP state register a1 to call VERHOGEN -- VERHOGEN use the semdAdd in reg a1 of the state saved*/
    ((state_PTR) BIOSDATAPAGE)->s_a1 = (int) &device_sem[devIdx];
    VERHOGEN();
    /* putting the process returned by V operation to unblocked_pcb */
    pcb_PTR unblocked_pcb = (pcb_PTR) ((state_PTR) BIOSDATAPAGE)->s_v0;

    /* if V operation failed to remove a pcb then return control to the current process*/
    if (unblocked_pcb == NULL){
        /* if there is no current process to return to either, call scheduler*/
        if (currentP == NULL){
            scheduler();
        }
        LDST((void *) BIOSDATAPAGE);
    }

    /* Place the stored off status code in the newly unblocked pcb’s v0 register.*/
    unblocked_pcb->p_s.s_v0 = savedDevRegStatus;

    /* Insert the newly unblocked pcb on the Ready Queue*/
    /* Done in VERHOGEN()*/

    /* Perform a LDST on the saved exception state*/
    LDST((void *) BIOSDATAPAGE);  
}

/*Process Local Timer (PLT) Interrupt*/
void process_local_timer_interrupts(){
    /* load new time into timer for PLT*/
    setTIMER(5000); 
    /* copy the processor state at the time of the exception into current process*/
    deep_copy_state_t(&(currentP->p_s), (state_PTR) BIOSDATAPAGE);
    /* update accumulated CPU time for the current process*/
    int interval_current;
    STCK(interval_current);
    currentP->p_time += 5000 - getTIMER();
    /* place current process on ready queue*/
    insertProcQ(&readyQ, currentP);
    scheduler();
}

void pseudo_clock_interrupts(){
    /* load interval timer with 100 miliseconds*/
    LDIT(100000);
    pcb_PTR unblocked_pcb;
    int *pseudo_clock_sem = &(device_sem[pseudo_clock_idx]);
    /*unblock all pcb blocked on the Pseudo-clock*/
    while (headBlocked(pseudo_clock_sem) != NULL){
        unblocked_pcb = helper_unblock_process(pseudo_clock_sem);
        insertProcQ(&readyQ, unblocked_pcb);
    }
    /* reset pseudo-clock semaphore to 0*/
    *(pseudo_clock_sem) = 0;
    state_PTR caller_process_state = (state_PTR) BIOSDATAPAGE;
    if (currentP == NULL){
        scheduler();
    }
    /* return contorl to the current process*/
    LDST((state_t *) BIOSDATAPAGE);
}

/* Non-Timer Interrupts */
void non_timer_interrupts(int intLineNo){

    /*  Calculate the address for Interrupting Devices Bit Map of that Line. */
    /*  After knowing which line specifically, this is how get the devices that have pending interrupt on that line
        Value at intLineDevBitMap as a hex has a bit as 1 if that device has interrupt pending */
    int *intLineDevBitMap = (int*) intDevBitMap(intLineNo);
    
    /* Boolean to later store whether the device has interrupt*/
    int devIntBool;
   
    /* address of register of device with interrupt pending*/
    device_t *intDevRegAdd;

    /* looping through every bit to see if the device has interrupt*/
    int devNo;
    for (devNo = 0; devNo < DEVPERINT; devNo ++){
        /* check in Interrupting Devices Bit Map to see if the device has interrupt */
        devIntBool = check_interrupt_device(devNo, intLineDevBitMap);

        if (devIntBool == TRUE){
            /* calculate address of register of device with interrupt pending to look into device status to check if read or write terminal*/
            intDevRegAdd = (device_t*) devAddrBase(intLineNo, devNo);

            if (intLineNo != 7 | ((intLineNo == 7)&(intDevRegAdd->d_status == 5))){ /* if it is not terminal device or is terminal and needing to write to terminal*/ /* for terminal device, data1 is trasm_command, data0 is transm_status, comman is recv_comman, status is recv_status */
                helper_terminal_write_other_device(intLineNo, devNo);
            }
            helper_terminal_read(intLineNo, devNo);
        }
    }
}



/* interupt exception handler function */
void interrupt_exception_handler(){
    int i;
    int lineIntBool;
    /* loop through the interrupt line for devices*/
    for (i = 0; i <= INTLINESCOUNT; i ++){
        lineIntBool = check_interrupt_line(i);
        /*
        Interrupt line 0 is reserved for inter-processor interrupts.
        Line 1 is reserved for the processor Local Timer interrupts.
        Line 2 is reserved for system-wide Interval Timer interrupts.
        Interrupt lines 3–7 are for monitoring interrupts from peripheral devices
        */
        if (lineIntBool == TRUE) {
            switch (i)
            {
            case INTERPROCESSORINT:
                /*processor interrupt -- pandos is only for uniprocessor -- this case will not happen -- can safely ignore -- delete later*/
                break;
            case PLTINT:
                /*processor local timer (PLT) interrupt*/
                process_local_timer_interrupts();
            case INTERVALTIMERINT:
                /*interval timer interrupt*/
                pseudo_clock_interrupts();
            case DISKINT:
            case FLASHINT:
            case NETWINT:
            case PRNTINT:
            case TERMINT:
                /*device interrupt*/
                non_timer_interrupts(TERMINT);
            default:
                break;
            }
        }
    }
}