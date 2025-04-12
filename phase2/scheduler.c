/*********************************SCHEDULER.C*******************************
 *  Scheduler Module
 *
 *  This module manages the scheduling of processes in the system. It manages
 *  scheduling the processes gets a chance to run using a round-robin scheduling 
 *  method.
 *  The function scheduler() selects the next process from the ready queue
 *  and gives it control.
 *
 *  The scheduler uses a queue to manage ready processes. When a process is selected to 
 *  run, its state is loaded using `LDST()`, and the processor timer is set to 5 
 *  milliseconds to ensure proper execution.
 *
 *  Modified by Phuong and Oghap on Feb 2025
 */

#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"

#include "initial.h"

#include "scheduler.h"


/**********************************************************
 *  scheduler()
 *
 *  This function determines the next process from the ready queue
 *  and schedules it for execution. If no process is ready, it
 *  determines the appropriate system action based on the process
 *  count and soft-block count.
 *
 *  The scheduler implements a round-robin scheduling method and
 *  sets each process to get an execution time by setting
 *  the processor timer to 5 milliseconds.
 *
 *  Parameters:
 *         
 *
 *  Returns:
 *
 **********************************************************/
void scheduler (){
    
    currentP = NULL;
    /* if the ready Q is empty */
    if (emptyProcQ(readyQ)){ 

        /* if the Process Count is zero */
        if (process_count == 0) {
            HALT();

        } else if (softBlock_count > 0){
            /* if Process Count > 0 and the Soft-block Count > 0 */

            /* get status, enable interrupt on current enable bit, disable PLT, enable Interrupt Mask */
            setSTATUS(0x0000ff01);
            WAIT();
        }else{
            /* if ProcessCount > 0 and softBlock_count = 0 */
            PANIC();
        }
    }
    
    /* remove from the Ready Queue */
    currentP = removeProcQ(&readyQ);

    /*Load 5 milisec on the PLT*/ 
    setTIMER(5000);

    /* pass in the address of current process processor state */
    LDST(&(currentP->p_s));
}