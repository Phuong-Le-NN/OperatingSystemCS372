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
    if (emptyProcQ(readyQ)){
        if (process_count == 0) {
            HALT();
        }
        if (process_count > 0 && softBlock_count > 0){
            setSTATUS();
            WAIT();
        }
    }
    
    currentP = removeBlocked(readyQ);
    LDIT(5); /*Load 5 milisec on the PLT*/
    LDST(&(currentP->p_s)); /*pass in the address of current process processor state*/
}