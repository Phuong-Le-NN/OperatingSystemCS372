/*********************************SYSSUPPORT.C*******************************
 *
 *  Implementation of the System Support Exception Handlers Module
 *
 *  This module defines exception handlers used by user processes
 *  to manage system calls, general exceptions, and program traps.
 *  It supports pass-up handling and interaction with the
 *  operating system kernel through the support structure.
 *
 *  The system support includes:
 *  - A SYSCALL exception handler that differentiates between valid
 *    system calls and illegal operations from user processes.
 *  - A Program Trap handler that deals with undefined or illegal
 *    instructions executed by a user process.
 *  - A General Exception handler that dispatches to appropriate
 *    handlers or terminates the process if the exception is unhandled.
 *
 *      Modified by Phuong and Oghap on March 2025
 */

#include "sysSupport.h"

/**********************************************************
 *  helper_check_string_outside_addr_space
 *
 *  Returns TRUE if the given string address falls outside
 *  the allowed logical address space for the user process.
 *
 *  Parameters:
 *         int strAdd – virtual address of the string
 *
 *  Returns:
 *         int – TRUE if address is invalid, FALSE otherwise
 **********************************************************/
int helper_check_string_outside_addr_space(int strAdd){
    if ((strAdd < KUSEG || strAdd > (LAST_USER_PAGE + PAGESIZE)) && (strAdd < UPROC_STACK_AREA || strAdd > (UPROC_STACK_AREA + PAGESIZE))){
        return TRUE;
    }
    return FALSE;
}

/**********************************************************
 *  helper_return_control
 *
 *  Advances the program counter and returns control to the
 *  user process by restoring its exception state.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *         
 **********************************************************/
void helper_return_control(support_t *passedUpSupportStruct){
    passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_pc += 4;
    LDST(&(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT]));
}

/**********************************************************
 *  program_trap_handler
 *
 *  Handles program trap exceptions by terminating the user
 *  process cleanly.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *         
 **********************************************************/
void program_trap_handler(support_t *passedUpSupportStruct, semd_t *heldSemd){
    /*release any mutexes the U-proc might be holding.
    perform SYS9 (terminate) the process cleanly.*/
    if (heldSemd != NULL){
        SYSCALL(4, heldSemd, 0, 0);
    }
    TERMINATE(passedUpSupportStruct);
}

/**********************************************************
 *  TERMINATE
 *
 *  Terminates a user process. Releases its occupied frames,
 *  marks pages invalid, and performs SYS2 to kill the process.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *         
 **********************************************************/
void TERMINATE(support_t *passedUpSupportStruct){
    /* Disable interrupts before touching shared structures */
    setSTATUS(getSTATUS() & (~IECBITON));
    int i;
    /* mark all of the frames it occupied as unoccupied */
    SYSCALL(3, &swapPoolSema4, 0, 0);
    for (i = 0; i < SWAP_POOL_SIZE; i++){
        if (swapPoolTable[i].ASID == passedUpSupportStruct->sup_asid){
            swapPoolTable[i].ASID = -1;
            swapPoolTable[i].pgNo = -1;
            swapPoolTable[i].matchingPgTableEntry = NULL;
        }
    }
    SYSCALL(4, &swapPoolSema4, 0, 0);

    /* Mark pages as invalid (clear VALID bit) */
    for (i = 0; i < PAGE_TABLE_SIZE; i++) {
        passedUpSupportStruct->sup_privatePgTbl[i].EntryLo &= ~(PFN_MASK + VBITON);
    }    

    /* Re-enable interrupts */
    setSTATUS(getSTATUS() | IECBITON);

    SYSCALL(4, &masterSemaphore, 0, 0);

    /* Terminate the process */
    SYSCALL(2, 0, 0, 0);  /* SYS2 */
}

/**********************************************************
 *  GET_TOD
 *
 *  Retrieves the current time-of-day from the system clock
 *  and stores it in the user process's return register.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *         
 **********************************************************/
void GET_TOD(support_t *passedUpSupportStruct){
    STCK(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0);
}

/**********************************************************
 *  WRITE_TO_PRINTER
 *
 *  Writes a string to the printer device character by character.
 *  Handles errors for invalid addresses or invalid string length.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *         
 **********************************************************/
void WRITE_TO_PRINTER(support_t *passedUpSupportStruct) {
    /*
    virtual address of the first character of the string to be transmitted in a1,
    the length of this string in a2
    */
    state_t *savedExcState = &(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT]);

    int devNo = passedUpSupportStruct->sup_asid - 1;
    device_t *printerDevAdd = devAddrBase(PRNTINT, devNo);

    /* Error: to write to a printer device from an address outside of the requesting U-proc’s logical address space*/
    /* Error: length less than 0*/
    /* Error: a length greater than 128*/

    if (helper_check_string_outside_addr_space(savedExcState->s_a1) || (savedExcState->s_a2 < STR_MIN) || (savedExcState->s_a2 > STR_MAX)){
        program_trap_handler(passedUpSupportStruct, NULL);
    }

    int mutexSemIdx = devSemIdx(PRNTINT, devNo, FALSE);
    SYSCALL(3, &(mutex[mutexSemIdx]), 0, 0);
    int i;
    int devStatus;
    for (i = 0; i < savedExcState->s_a2; i++){
        setSTATUS(getSTATUS() & (~IECBITON));
        printerDevAdd->d_data0 = *(((char *)savedExcState->s_a1) + i); /*calculate address and accessing the current char*/
        printerDevAdd->d_command = PRINTCHR;
        devStatus = SYSCALL(5, PRNTINT, devNo, 0); /*call SYSCALL WAITIO to block until interrupt*/
        setSTATUS(getSTATUS() | IECBITON);
        if (devStatus != READY) { /* operation ends with a status other than "Device Ready" -- this is printer, not terminal */
            savedExcState->s_v0 = - devStatus;
            break;
        }
    }

    if (devStatus == READY) { 
        savedExcState->s_v0 = i;;
    } else {
        savedExcState->s_v0 = - devStatus;
    }
    SYSCALL(4, &(mutex[mutexSemIdx]), 0, 0);
}

/**********************************************************
 *  WRITE_TO_TERMINAL
 *
 *  Writes a string to the terminal device. Sends each character
 *  one at a time using the TRANSMITCHAR command.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *         
 **********************************************************/
void WRITE_TO_TERMINAL(support_t *passedUpSupportStruct) {
    /*
    virtual address of the first character of the string to be transmitted in a1,
    the length of this string in a2
    */
    state_t *savedExcState = &(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT]);

    int devNo = passedUpSupportStruct->sup_asid - 1;
    device_t *termDevAdd = devAddrBase(TERMINT, devNo);

    /* Error: to write to a printer device from an address outside of the requesting U-proc’s logical address space*/
    /* Error: length less than 0*/
    /* Error: a length greater than 128*/
    if (helper_check_string_outside_addr_space(savedExcState->s_a1) || (savedExcState->s_a2 < STR_MIN) || (savedExcState->s_a2 > STR_MAX)){
        program_trap_handler(passedUpSupportStruct, NULL);
    }

    int mutexSemIdx = devSemIdx(TERMINT, devNo, FALSE);
    SYSCALL(3, &(mutex[mutexSemIdx]), 0, 0);
    int i;
    int transmStatus;
    for (i = 0; i < savedExcState->s_a2; i++){
        setSTATUS(getSTATUS() & (~IECBITON));
        termDevAdd->t_transm_command = (*(((char *) savedExcState->s_a1) + i) << TRANS_COMMAND_SHIFT) + TRANSMIT_COMMAND;
        transmStatus = SYSCALL(5, TERMINT, devNo, FALSE); /*call SYSCALL WAITIO to block until interrupt*/ 
        setSTATUS(getSTATUS() | IECBITON);
        if ((transmStatus & STATUS_CHAR_MASK) != CHAR_TRANSMITTED) { /* operation ends with a status other than Character Transmitted */ 
            savedExcState->s_v0 = - transmStatus;
            break;
        }
    }

    if ((transmStatus & STATUS_CHAR_MASK) == CHAR_TRANSMITTED) {
        savedExcState->s_v0 = i;;
    } else {
        savedExcState->s_v0 = -transmStatus;
    }
    SYSCALL(4, &(mutex[mutexSemIdx]), 0, 0);
}

/**********************************************************
 *  READ_FROM_TERMINAL
 *
 *  Reads characters from the terminal input device until newline.
 *  Stores characters in the buffer provided by the user process.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *         
 **********************************************************/
void READ_FROM_TERMINAL(support_t *passedUpSupportStruct) {
    /* Get the terminal device register */
    state_t *savedExcState = &(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT]);

    int devNo = passedUpSupportStruct->sup_asid - 1;
    device_t *termDevAdd = devAddrBase(TERMINT, devNo);

    /* Error: to write to a printer device from an address outside of the requesting U-proc’s logical address space*/
    /* Error: length less than 0*/
    /* Error: a length greater than 128*/
    if (helper_check_string_outside_addr_space(savedExcState->s_a1) || savedExcState->s_a2 < STR_MIN || (savedExcState->s_a2 > STR_MAX)){
        program_trap_handler(passedUpSupportStruct, NULL);
    }

    int mutexSemIdx = devSemIdx(TERMINT, devNo, TRUE);
    SYSCALL(3, &(mutex[mutexSemIdx]), 0, 0);

    char *stringAdd = savedExcState->s_a1;

    int i = 0;
    int recvStatusField;
    int recvStatus;
    char recvChar = 'a';
    while (recvChar != NEW_LINE){
        setSTATUS(getSTATUS() & (~IECBITON));
        termDevAdd->t_recv_command = RECEIVE_COMMAND;
        recvStatusField = SYSCALL(5, TERMINT, devNo, TRUE); /*call SYSCALL WAITIO to block until interrupt*/
        setSTATUS(getSTATUS() | IECBITON);
        recvChar = (recvStatusField & RECEIVE_CHAR_MASK) >> RECEIVE_COMMAND_SHIFT;
        recvStatus = recvStatusField & STATUS_CHAR_MASK;
        stringAdd[i] = recvChar; /* write the char into the string buffer array */
        i++;
        if (recvStatus != CHAR_RECIEVED) { /* operation ends with a status other than Character Received */ 
            savedExcState->s_v0 = -recvStatus;
            break;
        }
    }

    if (recvStatus == CHAR_RECIEVED) {
        savedExcState->s_v0 = i;
    } else {
        savedExcState->s_v0 = -recvStatus;
    }
    SYSCALL(4, &(mutex[mutexSemIdx]), 0, 0);

}

/**********************************************************
 *  syscall_handler
 *
 *  Dispatches system calls from user processes. Handles SYS9 to SYS13.
 *  If an unknown system call is encountered, invokes the trap handler.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *         
 **********************************************************/
void syscall_handler(support_t *passedUpSupportStruct) {
    switch (passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_a0){
        case 9:
            TERMINATE(passedUpSupportStruct);
        case 10:
            GET_TOD(passedUpSupportStruct);
            helper_return_control(passedUpSupportStruct);
        case 11:
            WRITE_TO_PRINTER(passedUpSupportStruct);
            helper_return_control(passedUpSupportStruct);
        case 12:
            WRITE_TO_TERMINAL(passedUpSupportStruct);
            helper_return_control(passedUpSupportStruct);
        case 13:
            READ_FROM_TERMINAL(passedUpSupportStruct);
            helper_return_control(passedUpSupportStruct);
        default: /*the case where the process tried to do SYS 8- in user mode*/
            program_trap_handler(passedUpSupportStruct, NULL);
    }
}

/**********************************************************
 *  general_exception_handler
 *
 *  Determines the type of general exception and routes it to
 *  either the syscall handler or program trap handler.
 *
 *  Parameters:
 *        
 *
 *  Returns:
 *         
 **********************************************************/
void general_exception_handler() { 
    support_t *passedUpSupportStruct = SYSCALL(8, 0, 0, 0);
    /* like in phase2 how we get the exception code*/
    int excCode = CauseExcCode(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_cause);
    /* examine the sup_exceptState's Cause register ... pass control to either the Support Level's SYSCALL exception handler, or the support Level's Program Trap exception handler */
    if (excCode == SYS){
        syscall_handler(passedUpSupportStruct);
    }
        program_trap_handler(passedUpSupportStruct, NULL);
}