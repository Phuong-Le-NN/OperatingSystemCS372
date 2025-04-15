/*********************************SYSSUPPORT.C*******************************
 * 
 *  General exception handler. 
 *  SYSCALL exception handler. 
 *  Program Trap exception handler.
 *
 *      Modified by Phuong and Oghap on March 2025
 */

#include "sysSupport.h"
/**************************************************************************************************************** 
 * return TRUE if string address is NOT valid
*/
int helper_check_string_outside_addr_space(int strAdd){
    if ((strAdd < 0x80000000 | strAdd > 0x8001E000 + 0x1000) & (strAdd < 0xBFFFF000 | strAdd > 0xBFFFF000 + 0x1000)){
        return TRUE;
    }
    return FALSE;
}

void helper_return_control(support_t *passedUpSupportStruct){
    passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_pc += 4;
    LDST(&(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT]));
}

void program_trap_handler(support_t *passedUpSupportStruct){
    /*
    1. release any mutexes the U-proc might be holding.
    2. perform SYS9 (terminate) the process cleanly.
    */
    TERMINATE(passedUpSupportStruct);
}

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
    for (i = 0; i < 32; i++) {
        passedUpSupportStruct->sup_privatePgTbl[i].EntryLo &= ~0xFFFFF200;
    }    

    /* Re-enable interrupts */
    setSTATUS(getSTATUS() | IECBITON);

    /* 4.10 Small Support Level Optimizations */
    SYSCALL(4, &masterSemaphore, 0, 0);

    /* Terminate the process */
    SYSCALL(2, 0, 0, 0);  /* SYS2 */
}

void GET_TOD(support_t *passedUpSupportStruct){
    /* POPS 4.1.2 */
    STCK(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0);
}

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

    if (helper_check_string_outside_addr_space(savedExcState->s_a1) || (savedExcState->s_a2 < 0) || (savedExcState->s_a2 > 128)){
        SYSCALL(9, 0, 0, 0);
    }

    int mutexSemIdx = devSemIdx(PRNTINT, devNo, FALSE);
    SYSCALL(3, &(mutex[mutexSemIdx]), 0, 0);
    int i;
    int devStatus;
    for (i = 0; i < savedExcState->s_a2; i++){
        setSTATUS(getSTATUS() & (~IECBITON));
        printerDevAdd->d_data0 = *(((char *)savedExcState->s_a1) + i); /*calculate address and accessing the current char*/
        printerDevAdd->d_command = 2; /* command PRINTCHR */
        devStatus = SYSCALL(5, PRNTINT, devNo, 0); /*call SYSCALL WAITIO to block until interrupt*/
        setSTATUS(getSTATUS() | IECBITON);
        if (devStatus != 1) { /* operation ends with a status other than "Device Ready" -- this is printer, not terminal */
            savedExcState->s_v0 = - devStatus;
            break;
        }
    }

    if (devStatus == 1) { /* "Device Ready" */
        savedExcState->s_v0 = i;;
    } else {
        savedExcState->s_v0 = - devStatus;
    }
    SYSCALL(4, &(mutex[mutexSemIdx]), 0, 0);
}

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
    if (helper_check_string_outside_addr_space(savedExcState->s_a1) || savedExcState->s_a2 < 0 || savedExcState->s_a2 > 128){
        SYSCALL(9, 0, 0, 0);
    }

    int mutexSemIdx = devSemIdx(TERMINT, devNo, FALSE);
    SYSCALL(3, &(mutex[mutexSemIdx]), 0, 0);
    int i;
    int transmStatus;
    for (i = 0; i < savedExcState->s_a2; i++){
        setSTATUS(getSTATUS() & (~IECBITON));
        termDevAdd->t_transm_command = (*(((char *) savedExcState->s_a1) + i) << 8) + 2; /*calculate address and accessing the current char, shift to the right position and add the TRANSMITCHAR command*/
        transmStatus = SYSCALL(5, TERMINT, devNo, FALSE); /*call SYSCALL WAITIO to block until interrupt*/ 
        setSTATUS(getSTATUS() | IECBITON);
        if ((transmStatus & 0xff) != 5) { /* operation ends with a status other than Character Transmitted */ /* Character Transmitted -- what we return in interrupt*/
            savedExcState->s_v0 = - transmStatus;
            break;
        }
    }

    if ((transmStatus & 0xff) == 5) {
        savedExcState->s_v0 = i;;
    } else {
        savedExcState->s_v0 = -transmStatus;
    }
    SYSCALL(4, &(mutex[mutexSemIdx]), 0, 0);
}

void READ_FROM_TERMINAL(support_t *passedUpSupportStruct) {
    /* Get the terminal device register
     * POPS 5.3.1 — devAddrBase(line, devNo) gives address of device register
     * Pandos assigns terminal device (receive part) per ASID-1
     */
    state_t *savedExcState = &(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT]);

    int devNo = passedUpSupportStruct->sup_asid - 1;
    device_t *termDevAdd = devAddrBase(TERMINT, devNo);

    /* Error: to write to a printer device from an address outside of the requesting U-proc’s logical address space*/
    /* Error: length less than 0*/
    /* Error: a length greater than 128*/
    if (helper_check_string_outside_addr_space(savedExcState->s_a1) || savedExcState->s_a2 < 0 || savedExcState->s_a2 > 128){
        SYSCALL(9, 0, 0, 0);
    }

    int mutexSemIdx = devSemIdx(TERMINT, devNo, TRUE);
    SYSCALL(3, &(mutex[mutexSemIdx]), 0, 0);

    char *stringAdd = savedExcState->s_a1;

    int i = 0;
    int recvStatusField;
    int recvStatus;
    char recvChar = 'a';
    while (recvChar != 10){
        setSTATUS(getSTATUS() & (~IECBITON));
        termDevAdd->t_recv_command = 2; /* RECEIVECHAR command*/
        recvStatusField = SYSCALL(5, TERMINT, devNo, TRUE); /*call SYSCALL WAITIO to block until interrupt*/
        setSTATUS(getSTATUS() | IECBITON);
        recvChar = (recvStatusField & 0x0000FF00) >> 8;
        recvStatus = recvStatusField & 0x000000FF;
        stringAdd[i] = recvChar; /* write the char into the string buffer array */
        i++;
        if (recvStatus != 5) { /* operation ends with a status other than Character Received */ /* Character Received -- what we return in interrupt*/
            savedExcState->s_v0 = -recvStatus;
            break;
        }
    }

    if (recvStatus == 5) {
        savedExcState->s_v0 = i;
    } else {
        savedExcState->s_v0 = -recvStatus;
    }
    SYSCALL(4, &(mutex[mutexSemIdx]), 0, 0);

}


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
            program_trap_handler(passedUpSupportStruct);
    }
}

void general_exception_handler() { 
    support_t *passedUpSupportStruct = SYSCALL(8, 0, 0, 0);
    /* like in phase2 how we get the exception code*/
    int excCode = CauseExcCode(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_cause);
    /* examine the sup_exceptState's Cause register ... pass control to either the Support Level's SYSCALL exception handler, or the support Level's Program Trap exception handler */
    /* 8 is the ExcCode for Sys*/
    if (excCode == 8){
        syscall_handler(passedUpSupportStruct);
    }
        program_trap_handler(passedUpSupportStruct);
}