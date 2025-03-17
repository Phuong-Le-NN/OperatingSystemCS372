/*********************************VMSUPPORT.C*******************************
 * 
 *  This module implements the TLB exception handler (The Pager). 
 *  Since reading and writing to each U-proc’s flash device is limited to supporting paging, 
 *  this module should also contain the function(s) for reading and writing flash devices.
 * 
 *  The Swap Pool table and Swap Pool semaphore are local to this module. 
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

#define ENTRYHI_MASK 0xFFFFF000   // Mask to extract VPN (bits 31-12)
#define PAGE_SHIFT   12           // Shift right to get the page number

/**********************************************************
 *  uTLB_RefillHandler()
 * 
 **********************************************************/
void uTLB_RefillHandler() {

    state_t *exceptionState = (state_t *)BIOSDATAPAGE;  // Get saved exception state
    int exceptionType = CauseExcCode(exceptionState->s_cause);  // Extract TLB exception type
    int missingVPN = (exceptionState->s_entryHI & ENTRYHI_MASK) >> PAGE_SHIFT;  // Extract Vitual Page Number

    if (exceptionType == 2 || exceptionType == 3) { // TLBL or TLBS
        pte_t *pte = getPageTableEntry(missingVPN);  // Find Page Table entry

        setENTRYHI(pte->EntryHi);
        setENTRYLO(pte->EntryLo);
        TLBWR();  // Write new entry to TLB

        LDST(exceptionState);  // Resume execution
    } else {
        // If it's an unexpected exception (TLB-Modification), treat it as a program trap
        programTrapHandler();
    }
}