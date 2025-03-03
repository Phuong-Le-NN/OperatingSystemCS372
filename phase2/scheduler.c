/*********************************SCHEDULER.C*******************************
 *
 * 
 *      Modified by Phuong and Oghap on Feb 2025
 */

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "initial.c"
#include "/usr/include/umps3/umps/libumps.h"

/*
In scheduler:
    - get pcb from the readyque
    - check if queue empty or not
    - then, LDST(runningProcQ -> p_state)
*/

void scheduler (){

    /*if the ready Q is empty*/
    if (emptyProcQ(readyQ)){ /*remove unnecessary conditions -- bad coding styles*/
        if (process_count == 0) {
            HALT();
        }
        if (process_count > 0 && softBlock_count > 0){
            unsigned int current_status_reg = getSTATUS();
            current_status_reg = enable_IEc(current_status_reg);
            current_status_reg = disable_TE(current_status_reg);
            setSTATUS(current_status_reg);
            WAIT();
        }
        if (process_count > 0 && softBlock_count == 0){
            PANIC();
        }
    }
    
    currentP = removeProcQ(readyQ);
    setTIMER(5000);                         /*Load 5 milisec on the PLT*/ /* need to check again -- LDIT is load the interval timer not PLT? -- is setTIMER the right one?*/
    LDST(&(currentP->p_s));             /*pass in the address of current process processor state*/
}