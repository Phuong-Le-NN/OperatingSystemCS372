/*********************************SYSSUPPORT.C*******************************
 * 
 *  General exception handler. 
 *  SYSCALL exception handler. 
 *  Program Trap exception handler.
 *
 *      Modified by Phuong and Oghap on March 2025
 */


 #include "/usr/include/umps3/umps/libumps.h"

 #include "../h/pcb.h"
 #include "../h/asl.h"
 #include "../h/types.h"
 #include "../h/const.h"
   
 #include "../phase2/initial.h"
 #include "../phase2/scheduler.h"
 #include "../phase2/exceptions.h"
 #include "../phase2/interrupts.h"

 extern int masterSemaphore;
 extern int* mutex;
 extern int swapPoolSema4;
 extern swapPoolFrame_t swapPoolTable[8 * 2];


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
    What causes a program trap.
    That the handler should terminate the process.
    If the process holds any mutual exclusion semaphores (like for flash or swap), they must be released first.

    this should:
    1. release any mutexes the U-proc might be holding.
    2. perform SYS9 (terminate) the process cleanly.
    */
    TERMINATE(passedUpSupportStruct);
 }

 void TERMINATE(support_t *passedUpSupportStruct){
    /* Disable interrupts before touching shared structures */
    setSTATUS(getSTATUS() & (~IECBITON));

    /* mark all of the frames it occupied as unoccupied */
    SYSCALL(3, &swapPoolSema4, 0, 0);
    int i;
    for (i = 0; i < 8 * 2; i++){
        if (swapPoolTable[i].ASID == (passedUpSupportStruct->sup_privatePgTbl[i].EntryHi >> 6) & 0x000000FF){
            swapPoolTable[i].ASID = -1;
            swapPoolTable[i].pgNo = -1;
            swapPoolTable[i].matchingPgTableEntry = NULL;
        }
    }
    SYSCALL(4, &swapPoolSema4, 0, 0);

    /* Mark pages as invalid (clear VALID bit) */
    for (i = 0; i < 32; i++) {
        if (passedUpSupportStruct->sup_privatePgTbl[i].EntryLo & 0x00000200) {
            passedUpSupportStruct->sup_privatePgTbl[i].EntryLo &= ~0x00000200;
        }
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
     int stringOutsideAddSpace = helper_check_string_outside_addr_space(savedExcState->s_a1);
     /* Error: length less than 0*/
     int negStringLen = (savedExcState->s_a2 < 0)? TRUE:FALSE;
     /* Error: a length greater than 128*/
     int oversizeStringLen = (savedExcState->s_a2 > 128)? TRUE:FALSE;
 
     if (stringOutsideAddSpace || negStringLen || oversizeStringLen){
         SYSCALL(9, 0, 0, 0);
     }
 
     int mutexSemIdx = devSemIdx(PRNTINT, devNo, FALSE);
     SYSCALL(3, &(mutex[mutexSemIdx]), 0, 0);
     int i;
     for (i = 0; i < savedExcState->s_a2; i++){
         setSTATUS(getSTATUS() & (~IECBITON));
         printerDevAdd->d_data0 = *((char *) (savedExcState->s_a1 + i)); /*calculate address and accessing the current char*/
         printerDevAdd->d_command = 2; /* command PRINTCHR */
         SYSCALL(5, PRNTINT, devNo, 0); /*call SYSCALL WAITIO to block until interrupt*/
         if (printerDevAdd->d_status != 1) { /* operation ends with a status other than "Device Ready" */
            savedExcState->s_v0 = - printerDevAdd->d_status;
             return;
         }
         setSTATUS(getSTATUS() | IECBITON);
     }
 
     if (printerDevAdd->d_status == 1) { /* "Device Ready" */
        savedExcState->s_v0 = savedExcState->s_a2;;
     } else {
        savedExcState->s_v0 = - printerDevAdd->d_status;
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
     int stringOutsideAddSpace = helper_check_string_outside_addr_space(savedExcState->s_a1);
     /* Error: length less than 0*/
     int negStringLen = (savedExcState->s_a2 < 0)? TRUE:FALSE;
     /* Error: a length greater than 128*/
     int oversizeStringLen = (savedExcState->s_a2 > 128)? TRUE:FALSE;
 
     if (stringOutsideAddSpace || negStringLen || oversizeStringLen){
         SYSCALL(9, 0, 0, 0);
     }
 
     int mutexSemIdx = devSemIdx(TERMINT, devNo, FALSE);
     SYSCALL(3, &(mutex[mutexSemIdx]), 0, 0);
     /* add dev mutex sema4 once init proc is done*/
     int i;
     for (i = 0; i < savedExcState->s_a2; i++){
         setSTATUS(getSTATUS() & (~IECBITON));
         termDevAdd->t_transm_command = *((char *) (savedExcState->s_a1 + i)) << 7 + 2; /*calculate address and accessing the current char, shift to the right position and add the TRANSMITCHAR command*/
         SYSCALL(5, TERMINT, devNo, FALSE); /*call SYSCALL WAITIO to block until interrupt*/
         if (termDevAdd->t_transm_status != 5) { /* operation ends with a status other than "Device Ready" */
            savedExcState->s_v0 = - termDevAdd->t_transm_status;
             return;
         }
         setSTATUS(getSTATUS() | IECBITON);
     }
 
     if (termDevAdd->t_transm_status == 5) { /* "Device Ready" */
        savedExcState->s_v0 = savedExcState->s_a2;;
     } else {
        savedExcState->s_v0 = - termDevAdd->t_transm_status;
     }
     SYSCALL(4, &(mutex[mutexSemIdx]), 0, 0);
 }
 
 /* Need to add Gain mutual exclusion on terminal receive NEED TO FIX */
 void READ_FROM_TERMINAL(support_t *passedUpSupportStruct) {
    /* Get the terminal device register
     * POPS 5.3.1 — devAddrBase(line, devNo) gives address of device register
     * Pandos assigns terminal device (receive part) per ASID-1
     */
    memaddr virtAddr = passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_a1;  /* a1 */

    /* Error if address is not in kuseg */
    if (virtAddr > 0xC0000000) {
        TERMINATE(passedUpSupportStruct);  /* terminate the U-proc */
    }
    
    int asid = passedUpSupportStruct->sup_asid;
    device_t *termRecvReg = (device_t *) devAddrBase(TERMINT, asid - 1);  /* receive part of terminal */
    
    /* Send read command POPS 5.4 */
    termRecvReg->d_command = 2;
    
    /* Block until read completes */
    SYSCALL(5, TERMINT, asid - 1, 0);
    
    /* Extract number of characters received from upper byte (POPS 5.3.3) */
    unsigned int status = termRecvReg->d_status;
    int charsReceived = (status >> 8) & 0xFF;
    
    if ((status & 0xFF) == 5) {
        /* Copy received characters into user buffer
         * Note: max 128 characters (Pandos 4.7.5)
         */
        char *userBuffer = (char *) virtAddr;
        char *sourceBuffer = (char *) termRecvReg->d_data0;
    
        for (int i = 0; i < charsReceived && i < 128; i++) {
            userBuffer[i] = sourceBuffer[i];
        }

        /* Return number of characters received in v0 (POPS 4.1.2) */
        passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = charsReceived;  /* v0 */
    } else {
        /* On error, return negative status (Pandos 4.7.5) */
        passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = -(status & 0xFF);
    }
    
    /* Return control happens after! */
    passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_t9 += 4;
 }

 void syscall_handler(support_t *passedUpSupportStruct) {
    switch (passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_a0){
        case 9:
            TERMINATE(passedUpSupportStruct);
            helper_return_control(passedUpSupportStruct);
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
        default: /*should never reach this case*/
            program_trap_handler(passedUpSupportStruct);
            helper_return_control(passedUpSupportStruct);
    }
}

 void general_exception_handler() { 
     support_t *passedUpSupportStruct = SYSCALL(8, 0, 0, 0);
     if (passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_a0 >= 9 && passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_a0 <= 13){
         syscall_handler(passedUpSupportStruct);
     }
         program_trap_handler(passedUpSupportStruct);
 }