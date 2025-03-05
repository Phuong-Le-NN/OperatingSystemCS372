/*********************************SCHEDULER.C*******************************
 *
 * 
 *      Modified by Phuong and Oghap on Feb 2025
 */
#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"

#include "initial.h"

#include "scheduler.h"

/*
In scheduler:
    - get pcb from the readyque
    - check if queue empty or not
    - then, LDST(runningProcQ -> p_state)
*/

/* The Scheduler does...
    1. Remove the pcb from the head of the Ready Queue and store the pointer to the pcb 
        in the Current Process field.
    2. Load 5 milliseconds on the PLT. [Section 4.1.4-pops]
    3. Perform a Load Processor State (LDST) on the processor state stored in pcb
        of the Current Process (p_s).
*/

void debug(int a0, int a1, int a2, int a3){
}

void scheduler (){

    debug(2,2,3,4);

    /* if the ready Q is empty */
    if (emptyProcQ(readyQ)){ 

        /* case for when the Process Count is zero */
        if (process_count == 0) {
            HALT();

        } else if (softBlock_count > 0){
            /* if process count is not equal to 0, then it must be greater than 0 */
            /* Thus, this case is checking for the Process Count > 0 and the Soft-block Count > 0 */

            unsigned int current_status_reg = getSTATUS();
            current_status_reg = enable_IEc(current_status_reg);
            current_status_reg = disable_TE(current_status_reg);
            setSTATUS(current_status_reg);
            WAIT();

        }else{
            /* if process count is not equal to 0, then it must be greater than 0 */
            /* if soft block count is greater than 0, then it must be equal to 0 */
            /* Thus, this case is for when ProcessCount > 0 and theSoft- block Count is zero*/
            PANIC();
        }
    }
    
    currentP = removeProcQ(&readyQ);

    /*Load 5 milisec on the PLT*/ 
    setTIMER(5000);

    /* pass in the address of current process processor state */
    LDST(&(currentP->p_s));
}