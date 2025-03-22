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

void helper_return_control(support_t *passedUpSupportStruct){
    passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_pc += 4;
    LDST(&(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT]));
}

void program_trap_handler(){

}

void WRITE_TO_PRINTER(support_t *passedUpSupportStruct) {
    /*
    virtual address of the first character of the string to be transmitted in a1,
    the length of this string in a2
    */
    state_t savedExcState = passedUpSupportStruct->sup_exceptState[GENERALEXCEPT];

    int devNo = passedUpSupportStruct->sup_asid - 1;
    device_t *printerDevAdd = devAddrBase(PRNTINT, devNo);

    /* Error: to write to a printer device from an address outside of the requesting U-proc’s logical address space*/
    int stringOutsideAddSpace = (savedExcState.s_a1 < KSEG2 | savedExcState.s_a1 > KUSEG)? TRUE:FALSE;
    /* Error: length less than 0*/
    int negStringLen = (savedExcState.s_a2 < 0)? TRUE:FALSE;
    /* Error: a length greater than 128*/
    int oversizeStringLen = (savedExcState.s_a2 > 128)? TRUE:FALSE;

    if (stringOutsideAddSpace || negStringLen || oversizeStringLen){
        SYSCALL(9, 0, 0, 0);
    }

    /* add dev mutex sema4 once init proc is done*/
    int i;
    for (i = 0; i < savedExcState.s_a2; i++){
        printerDevAdd->d_data0 = *((char *) (savedExcState.s_a1 + i)); /*calculate address and accessing the current char*/
        printerDevAdd->d_command = 2; /* command PRINTCHR */
        SYSCALL(5, PRNTINT, devNo, 0); /*call SYSCALL WAITIO to block until interrupt*/
        if (printerDevAdd->d_status != 1) { /* operation ends with a status other than "Device Ready" */
            savedExcState.s_v0 = - printerDevAdd->d_status;
            return;
        }
    }

    if (printerDevAdd->d_status == 1) { /* "Device Ready" */
        savedExcState.s_v0 = savedExcState.s_a2;;
    } else {
        savedExcState.s_v0 = - printerDevAdd->d_status;
    }
}

void syscall_handler(support_t *passedUpSupportStruct) {
    switch (passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_a0){
        case 9:
            TERMINATE();
            helper_return_control(passedUpSupportStruct);
        case 10:
            GET_TOD();
            helper_return_control(passedUpSupportStruct);
        case 11:
            WRITE_TO_PRINTER(passedUpSupportStruct);
            helper_return_control(passedUpSupportStruct);
        case 12:
            WRITE_TO_TERMINAL();
            helper_return_control(passedUpSupportStruct);
        case 13:
            READ_FROM_TERMINAL();
            helper_return_control(passedUpSupportStruct);
        default: /*should never reach this case*/
            program_trap_handler();
            helper_return_control(passedUpSupportStruct);
    }
}

void general_exception_handler() { 
    support_t *passedUpSupportStruct = SYSCALL(8, 0, 0, 0);
    if (passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_a0 >= 9 && passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_a0 <= 13){
        syscall_handler(passedUpSupportStruct);
    }
        program_trap_handler();
}