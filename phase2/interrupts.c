/*********************************INTERRUPTS.C*******************************
 *
 * 
 *      Modified by Phuong and Oghap on Feb 2025
 */

#include "interrupts.h"
#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "initial.c"
#include "exceptions.c"
#include "/usr/include/umps3/umps/libumps.h"

#define IPBITSPOS       8
#define INTLINESCOUNT   8

/* Non-Timer Interrupts */
void non_timer_interrupts(int intLineNo){
    /* after knowing which line specifically, this is how get the devices that have pending interrupt on that line*/
    int *devRegAdd = intDevBitMap(intLineNo);
    
    /* devRegAdd is address to the Interrupt Device Bit Map, value at that address as a hex has a bit as 1 if that device has interrupt pending*/
    int devNo;
    int devIntBool;
   
    /* address of register of device with interrupt pending*/
    device_t *intDevRegAdd;

    /* a variable to deep copy the device register to*/
    device_t *savedDevRegAdd;
    int devIdx;
    pcb_PTR unblocked_pcb;

    for (devNo = 0; devNo < DEVPERINT; devNo ++){
        devIntBool = check_interrupt_device(devNo, devRegAdd);
        if (devIntBool == TRUE){
            /* calculate the address of the device register*/
            intDevRegAdd = devAddrBase(intLineNo, devNo);
            /* save the register device */
            deep_copy_device_t(savedDevRegAdd, intDevRegAdd);
            /* putting the ACK into the device register?*/
            intDevRegAdd->d_command = ACK;
            if (intLineNo == 7){ /* if device interupt is transmitted*/
                /* for terminal device, data1 is trasm_command, data0 is transm_status, comman is recv_comman, status is recv_status*/
                if (savedDevRegAdd->d_data0 == 5){
                    intDevRegAdd->d_data1 = ACK;
                    devIdx = devSemIdx(intLineNo, devNo, 1);
                    unblocked_pcb = removeBlocked(device_sem[devIdx]);
                    insertProcQ(&readyQ, unblocked_pcb);
                    LDST(BIOSDATAPAGE);
                }
                /* for terminal reciever sub device, handling like other device?*/
            }
            intDevRegAdd->d_command = ACK;
            devIdx = devSemIdx(intLineNo, devNo, 0);
            ((state_t*) BIOSDATAPAGE)->s_a1 = &device_sem[devIdx];
            VERHOGEN();
            /* putting the process returned by V operation to unblocked_pcb */
            unblocked_pcb = ((state_t*) BIOSDATAPAGE)->s_v0;
            /* if V operation failed to remove a pcb then return control to the current process*/
            if (unblocked_pcb == NULL){
                /* if there is no current process to return to either, call scheduler*/
                if (currentP == NULL){
                    scheduler();
                }
                LDST(currentP);
            }
            LDST(BIOSDATAPAGE);
        }
    }
}


/*Process Local Timer (PLT) Interrupt*/
void process_local_timer_interrupts(){
    /* load new time into timer for PLT*/
    setTIMER(5000); 
    /* copy the processor state at the time of the exception into current process*/
    deep_copy_state_t(&(currentP->p_s), BIOSDATAPAGE);
    /* update accumulated CPU time for the current process*/
    int interval_current;
    STCK(interval_current);
    currentP->p_time += interval_current - interval_start;
    /* place current process on ready queue*/
    insertProcQ(&readyQ, currentP);
    scheduler();
}

void pseudo_clock_interrupts(){
    /* load interval timer with 100 miliseconds*/
    LDIT(100000);
    pcb_PTR unblocked_pcb;
    semd_t *pseudo_clock_sem = &(device_sem[pseudo_clock_idx]);
    /*unblock all pcb blocked on the Pseudo-clock*/
    while (headBlocked(pseudo_clock_sem) != NULL){
        unblocked_pcb = removeBlocked(pseudo_clock_sem);
        insertProcQ(readyQ, unblocked_pcb);
    }
    /* reset pseudo-clock semaphore to 0*/
    *(pseudo_clock_sem->s_semAdd) = 0;
    state_PTR caller_process_state = (state_t*) BIOSDATAPAGE;
    if (currentP == NULL){
        scheduler();
    }
    /* return contorl to the current process*/
    LDST(BIOSDATAPAGE);
}

/* interupt exception handler function */
void interrupt_exception_handler(){
    int i;
    int j;
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
                /*processor interrupt*/
                break;
            case PLTINT:
                /*processor local timer (PLT) interrupt*/
            case INTERVALTIMERINT:
                /*interval timer interrupt*/
            case DISKINT:
            case FLASHINT:
            case NETWINT:
            case PRNTINT:
            case TERMINT:
                /*device interrupt*/
            default:
                break;
            }
        }
    }
}

int check_interrupt_line(int idx){
    /*
    The function that takes in a line number and check if there is interrupt pending on that line
    Return TRUE or FALSE
    */
    /* get the IP bit from cause registers then shift right to get the interrupt line which*/
    /* as a hex, on bits indicate line with interrupt pending -- this is Cause.IP*/
    int IPLines = (getCause()&IPBITS) >> IPBITSPOS;
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
