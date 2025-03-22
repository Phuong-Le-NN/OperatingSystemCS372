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