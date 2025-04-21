/*********************************INTERRUPTS.C*******************************
 *  Interrupt Handling Module
 *
 *  This module handles system interrupts.
 *  There are three main types: processor local timer (PLT), pseudo-clock,
 *  and device interrupts. The function interrupt_exception_handler()
 *  checks which interrupt occurred and calls the right function.
 *
 *  The code uses functions to handle different types of interrupts.
 *  process_local_timer_interrupts() manages the PLT by resetting the timer
 *  and putting the current process back in the ready queue.
 *  pseudo_clock_interrupts() updates the pseudo-clock and unblocks waiting processes.
 *  non_timer_interrupts() checks which device caused an interrupt and processes it.
 *  It also has special handling for terminal devices using  helper_terminal_device()  and
 *  helper_non_terminal_device() .
 *
 *  The code uses arrays to store device semaphores and linked lists to manage process queues.
 *  It also updates the process state and may call the scheduler when needed.
 *
 *  Modified by Phuong and Oghap on Feb 2025
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

#define pseudo_clock_idx 48 /* pseudo clock semaphore in device semaphore array*/
#define IPBITSPOS 8         /* Interrupt pending bits position*/
#define INTLINESCOUNT 8     /* number of interrupt lines*/
#define REGWIDTH 32         /* register width*/

/**********************************************************
 *  helper_verhogen()
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
pcb_PTR helper_verhogen() {
	/*getting the sema4 address from register a1*/
	int *sema4 = ((state_PTR)BIOSDATAPAGE)->s_a1;

	pcb_PTR process_unblocked;
	(*sema4)++;

	if((*sema4) <= 0) {
		process_unblocked = removeBlocked(sema4);
		if(process_unblocked == NULL) {
			return NULL;
		}
		insertProcQ(&readyQ, process_unblocked);
		return process_unblocked;
	}
	return NULL;
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
	for(i = 0; i < STATEREGNUM; i++) {
		dest->s_reg[i] = src->s_reg[i];
	}
	dest->s_status = src->s_status;
}

/**********************************************************
 *  helper_check_interrupt_line()
 *
 *  Takes in a line number and check if there is interrupt pending
 *  on that line by checking on the cause register.
 *
 *  Parameters:
 *         int idx - line number
 *
 *  Returns:
 *          TRUE
 *          FALSE
 **********************************************************/
HIDDEN int helper_check_interrupt_line(int idx) {
	/* Get the IP bit from cause registers then shift right to get the interrupt line which*/
	/* in binary, 1 bits indicate line with interrupt pending -- this is Cause.IP*/
	int IPLines = (((state_PTR)BIOSDATAPAGE)->s_cause & IPBITS) >> IPBITSPOS;

	/* 31 as the size of int is 32 bits (4 bytes)*/
	if((IPLines << (REGWIDTH - 1 - idx)) >> (REGWIDTH - 1) == 0) {
		return FALSE;
	}
	return TRUE;
}

/**********************************************************
 *  helper_check_interrupt_device()
 *
 *  Checks if a device has interrupt pending by taking in a
 *  device number and address of the Line Interrupt
 *  Device Bit Map. Calculates by using device register bits.
 *
 *  Parameters:
 *         int idx - device number
 *         int* devRegAdd - address of the Line Interrupt
 *                          Device Bit Map
 *
 *  Returns:
 *          TRUE
 *          FALSE
 **********************************************************/
HIDDEN int helper_check_interrupt_device(int idx, int *devRegAdd) {
	/* loop through the devices on each line*/
	int devBits = *devRegAdd;
	/* 31 as the size of int is 32 bits (4 bytes)*/
	if((devBits << (REGWIDTH - 1 - idx)) >> (REGWIDTH - 1) == 0) {
		return FALSE;
	}
	return TRUE;
}

/**********************************************************
 *  helper_unblock_process()
 *
 *  Unblock 1 process, removes from the ASl,
 *  and decrements softblock count
 *
 *  Parameters:
 *         int *semdAdd - semaphore address
 *
 *  Returns:
 *         pcb_PTR - Pointer to the unblocked process
 **********************************************************/
HIDDEN pcb_PTR helper_unblock_process(int *semdAdd) {
	/* remove the process from ASL*/
	pcb_PTR unblocked_pcb = removeBlocked(semdAdd);

	if(unblocked_pcb != NULL) {
		softBlock_count -= 1;
	}
	return unblocked_pcb;
}

/**********************************************************
 *  helper_terminal_device()
 *
 *  Acknowledge interrupts from terminal read sub-device.
 *  Performs a V operation on the device semaphore
 *  and unblocks waiting process if needed
 *
 *  Parameters:
 *         int intLineNo - Interrupt line number
 *         int devNo  - Device Number
 *
 *  Returns:
 *
 **********************************************************/
HIDDEN void helper_terminal_device(int intLineNo, int devNo, int termRead) {
	/* Calculate the address for this device’s device register */
	device_t *intDevRegAdd = devAddrBase(intLineNo, devNo);
	int savedDevRegStatus;

	if(termRead) {
		/* Save off the status code from the device’s device register*/
		savedDevRegStatus = intDevRegAdd->t_recv_status; /* should be the char and 5 for CHAR RECEIVED*/ /* we want this because after acknowledged, anything will become Device Ready, even if the action did not succeed*/
		/* Acknowledge the outstanding interrupt */
		intDevRegAdd->t_recv_command = ACK;
	} else {
		/* Save off the status code from the device’s device register*/
		savedDevRegStatus = intDevRegAdd->t_transm_status; /* should be the char and 5 for CHAR TRANSMITTED*/ /* we want this because after acknowledged, anything will become Device Ready, even if the action did not succeed*/
		/* Acknowledge the outstanding interrupt */
		intDevRegAdd->t_transm_command = ACK;
	}

	/* Perform a V operation on the Nucleus maintained semaphore associated with this (sub)device.*/
	int devIdx = devSemIdx(intLineNo, devNo, termRead);

	/* put semdAdd into BIOSDATAPAGE state register a1 to call VERHOGEN -- VERHOGEN use the state in currentP*/
	((state_PTR)BIOSDATAPAGE)->s_a1 = &device_sem[devIdx];

	/* putting the process returned by V operation to unblocked_pcb */
	pcb_PTR unblocked_pcb = helper_verhogen();

	/* if V operation failed to remove a pcb then return control to the current process*/
	if(unblocked_pcb == NULL) {
		/* if there is no current process to return to either, call scheduler*/
		if(currentP == NULL) {
			scheduler();
		}
		LDST((state_PTR)BIOSDATAPAGE);
	}
	softBlock_count--;

	/* Place the stored off status code in the newly unblocked pcb’s v0 register.*/
	unblocked_pcb->p_s.s_v0 = savedDevRegStatus;

	/* Insert the newly unblocked pcb on the Ready Queue*/
	/* Done in helper_verhogen()*/
	/* if there is no current process to return to either, call scheduler*/
	if(currentP == NULL) {
		scheduler();
	}
	/* Perform a LDST on the saved exception state*/
	LDST((state_PTR)BIOSDATAPAGE);
}

/**********************************************************
 *  helper_non_terminal_device()
 *
 *  Aknowledge interrupts from terminal write sub-device
 *  or other devices. Performs a V operation on the device
 *  semaphore and unblocks waiting process if needed.
 *  When V operation fails to remove a pcb, it
 *  returns control to the current process.
 *
 *  Parameters:
 *         int intLineNo - Interrupt line number
 *         int devNo  - Device Number
 *
 *  Returns:
 *
 **********************************************************/
HIDDEN void helper_non_terminal_device(int intLineNo, int devNo) {
	/* Calculate the address for this device’s device register */
	device_t *intDevRegAdd = devAddrBase(intLineNo, devNo);

	/* Save off the status code from the device’s device register*/
	int savedDevRegStatus = intDevRegAdd->d_status;

	/* Acknowledge the outstanding interrupt */
	intDevRegAdd->d_command = ACK;

	/* Perform a V operation on the Nucleus maintained semaphore associated with this (sub)device.*/
	int devIdx = devSemIdx(intLineNo, devNo, FALSE);

	/* put semdAdd into BIOSDATAPAGE state register a1 to call VERHOGEN -- VERHOGEN use the semdAdd in reg a1 of the state saved*/
	((state_PTR)BIOSDATAPAGE)->s_a1 = &device_sem[devIdx];

	/* putting the process returned by V operation to unblocked_pcb */
	pcb_PTR unblocked_pcb = helper_verhogen();

	/* if V operation failed to remove a pcb then return control to the current process*/
	if(unblocked_pcb == NULL) {
		/* if there is no current process to return to either, call scheduler*/
		if(currentP == NULL) {
			scheduler();
		}
		LDST((state_PTR)BIOSDATAPAGE);
	}
	softBlock_count--;

	/* Place the stored off status code in the newly unblocked pcb’s v0 register.*/
	unblocked_pcb->p_s.s_v0 = savedDevRegStatus;

	/* Insert the newly unblocked pcb on the Ready Queue*/
	/* Done in helper_verhogen()*/

	if(currentP == NULL) {
		scheduler();
	}
	/* Perform a LDST on the saved exception state*/
	LDST((state_PTR)BIOSDATAPAGE);
}

/* Interrupts */

/**********************************************************
 *  process_local_timer_interrupts()
 *
 *  Resets timer to 5 miliseconds and copies the processor
 *  state from BIOS. Updates CPU time and moves the
 *  current process to the ready queue, then calls
 *  scheduler.
 *
 *  Parameters:
 *
 *  Returns:
 *
 **********************************************************/
HIDDEN void process_local_timer_interrupts() {
	/* load new time into timer for PLT*/
	setTIMER(5000);
	/* copy the processor state at the time of the exception into current process*/
	if(currentP != NULL) {
		deep_copy_state_t(&(currentP->p_s), (state_PTR)BIOSDATAPAGE);
	}
	/* update accumulated CPU time for the current process*/
	currentP->p_time += 5000 - getTIMER();
	/* place current process on ready queue*/
	insertProcQ(&readyQ, currentP);
	scheduler();
}

/**********************************************************
 *  pseudo_clock_interrupts()
 *
 *  Reloads timer and unblocks all pcb that was blocked on
 *  pseudo-clock. Resets the psuedo-clock semaphore and
 *  returns control to the current process.
 *
 *  Parameters:
 *
 *  Returns:
 *
 **********************************************************/
HIDDEN void pseudo_clock_interrupts() {
	/* load interval timer with 100 miliseconds*/
	LDIT(100000);
	int *pseudo_clock_sem = &(device_sem[pseudo_clock_idx]);
	pcb_PTR unblocked_pcb = helper_unblock_process(pseudo_clock_sem);
	/*unblock all pcb blocked on the Pseudo-clock*/
	while(unblocked_pcb != NULL) {
		insertProcQ(&readyQ, unblocked_pcb);
		unblocked_pcb = helper_unblock_process(pseudo_clock_sem);
	}
	/* reset pseudo-clock semaphore to 0*/
	*(pseudo_clock_sem) = 0;
	if(currentP == NULL) {
		scheduler();
	}
	/* return control to the current process*/
	LDST((state_t *)BIOSDATAPAGE);
}

/**********************************************************
 *  non_timer_interrupts()
 *
 *  Identifies the device that caused the interrupt and
 *  processes it accordingly. Checks in Interrupting Devices
 *  Bit Map of that line to see if the device has interrupt.
 *
 *  Parameters:
 *          int intLineNo - Line Number
 *  Returns:
 *
 **********************************************************/
HIDDEN void non_timer_interrupts(int intLineNo) {
	/*  Calculate the address for Interrupting Devices Bit Map of that Line. */

	/*  After knowing which line specifically, find which pending interrupt on that line
	    Value at Interrupting Devices Bit Map in binary has a bit as 1 if that device has interrupt pending */
	int *intLineDevBitMap = intDevBitMap(intLineNo);

	/* Boolean to later store whether the device has interrupt*/
	int devIntBool;

	/* address of register of device with interrupt pending*/
	device_t *intDevRegAdd;

	/* looping through every bit 0-7 to see if the device has interrupt*/
	int devNo;
	for(devNo = 0; devNo < DEVPERINT; devNo++) {
		/* check in Interrupting Devices Bit Map of that line to see if the device has interrupt */
		devIntBool = helper_check_interrupt_device(devNo, intLineDevBitMap);

		if(devIntBool == TRUE) {
			/* address of register of device*/
			intDevRegAdd = devAddrBase(intLineNo, devNo);

			if(intLineNo != 7) {
				helper_non_terminal_device(intLineNo, devNo);
			} else {
				int termRead = (((intDevRegAdd->d_status) & 0x000000FF) == 5);
				helper_terminal_device(intLineNo, devNo, termRead);
			}
		}
	}
}

/**********************************************************
 *  interrupt_exception_handler()
 *
 *  Determines the pending interrupt line and delegates to
 *  the appropriate handler such as process local timer,
 *  pseudo-clock, and devices interrupt handler.
 *
 *  Parameters:
 *
 *  Returns:
 *
 **********************************************************/
void interrupt_exception_handler() {
	int lineNum;
	int lineIntBool;
	/* loop through the interrupt line for devices*/
	for(lineNum = 0; lineNum < INTLINESCOUNT; lineNum++) {
		lineIntBool = helper_check_interrupt_line(lineNum);

		/*
		Interrupt line 0 is reserved for inter-processor interrupts.
		Line 1 is reserved for the processor Local Timer interrupts.
		Line 2 is reserved for system-wide Interval Timer interrupts.
		Interrupt lines 3–7 are for monitoring interrupts from peripheral devices
		*/
		if(lineIntBool == TRUE) {
			switch(lineNum) {
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
					non_timer_interrupts(lineNum);
				default:
					break;
			}
		}
	}
	if(currentP == NULL) {
		scheduler();
	}
	/* return control to the current process*/
	LDST((state_t *)BIOSDATAPAGE);
}
