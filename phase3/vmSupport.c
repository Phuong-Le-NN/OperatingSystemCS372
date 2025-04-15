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


#include "vmSupport.h"
void debugRefreshBackingStore(){

}

/**********************************************************
 *  
 **********************************************************/
void initSwapStruct(){/* 4.1 -- Address Translation */
    /*
    initSwapStructs which will do the work of initializing both the Swap Pool table and accompanying semaphore [4.11.2 Module Decomposition]
    */

    /* One Page Table per U-proc ..  This array should be added to the Support Structure (support t) that is pointed to by a Uproc’s pcb => added pte_t table in support_t struct in types.h*/
    /* The size of the Swap Pool should be set to two times UPROCMAX, where UPROCMAX is defined as the specific degree of multiprogramming to be supported: [1. . .8] -- this was done in header file*/
    
    /* Pandos Section 4.4.1 (page 48–49) */
    int i;
    for (i = 0; i < SWAP_POOL_SIZE; i++) {
        swapPoolTable[i].ASID = -1;
        swapPoolTable[i].pgNo = -1;
        swapPoolTable[i].matchingPgTableEntry = NULL;
    }
    swapPoolSema4 = 1;

}

/**********************************************************
 *  
 **********************************************************/
void uTLB_RefillHandler() { /* 4.3 -- The TLB-Refill event handler*/
    int missingVPN = (((state_PTR) BIOSDATAPAGE)->s_entryHI >> 12) & 0x000FFFFF; /* missingVPN is in range [0x80000 ... 0x8001E]*/

    /* Get the Page Table entry for page number p for the Current Process. This will be located in the Current Process’s Page Table*/

    int missingVPN_idx_in_pgTable = missingVPN % 32;

    support_t *currentSupport = currentP->p_supportStruct;
    pte_t *pte = &(currentSupport->sup_privatePgTbl[missingVPN_idx_in_pgTable]);

    /* Write this Page Table entry into the TLB*/
    setENTRYHI(pte->EntryHi);
    setENTRYLO(pte->EntryLo);
    TLBWR(); 

    LDST((state_PTR) BIOSDATAPAGE); 
}
/**********************************************************
 *  
 **********************************************************/
int page_replace() {   /* PANDOS 4.5.4 Replacement Algorithm */
    static int nextFrame = 0;
    
    /* Look for an empty frame */
    int i;
    for (i = 0; i < SWAP_POOL_SIZE; i = i + 1){
        if (swapPoolTable[i].ASID == -1) {  /* from PANDOS 4.4.1 Technical Point */
            if (i == nextFrame){ /* so that frame i doesn't get replace right away next time but only after cirulated*/
                nextFrame = (nextFrame + 1) % SWAP_POOL_SIZE;
            }
            return i;
        }
    }
    
    /* If no free frame, select the oldest one (FIFO) */ 
    int selectedFrame = nextFrame;
    nextFrame = (nextFrame + 1) % (SWAP_POOL_SIZE);  /* Move to next in circular order */

    return selectedFrame;
}

/**********************************************************
 * flash I/O function: read or write a page
 **********************************************************/
void read_write_flash(int pickedSwapPoolFrame, int devNo, int blockNo, int isRead) {
    int flashSemIdx = 8+devNo; /* this is read/write for terminal not flash => FALSE*/

    /* Get the device register address for the U-proc’s flash device */
    device_t *flashDevRegAdd = devAddrBase(FLASHINT, devNo);

    SYSCALL(3, &(mutex[flashSemIdx]), 0, 0);

    /* Disable interrupts to ensure to do atomically */
    setSTATUS(getSTATUS() & (~IECBITON));
    
    /* Write the physical memory address (start of frame) to DATA0 */ /* swap pool starts at 0x20020000 - pandos pg 48*/
    flashDevRegAdd->d_data0 = (0x20020000 + (pickedSwapPoolFrame * 4096));

    /* Choose the correct flash command */
    int flashCommand;
    if (isRead == 0) {
        flashCommand = 3;  /* FLASHWRITE */
    } else {
        flashCommand = 2;  /* FLASHREAD */
    }
    /* Write the command to COMMAND register */
    flashDevRegAdd->d_command = (blockNo << 8) | flashCommand;
    /* Block the process until the flash operation is complete */
    int flashStatus = SYSCALL(5, FLASHINT, devNo, 0);
    /* Re-enable interrupts */
    setSTATUS(getSTATUS() | IECBITON);

    SYSCALL(4, &(mutex[flashSemIdx]), 0, 0);

    if (flashStatus != 1){
        SYSCALL(9, 0, 0, 0);
    }
}


/**********************************************************
 *  
 **********************************************************/
void TLB_exception_handler() { /* 4.4.2 The Pager, Page Fault */
    /* 1. Obtain the pointer to the Current Process’s Support Structure: SYS8. */
    support_t *currentSupport = SYSCALL(8, 0, 0, 0);

    /* 2. Determine the cause of the TLB exception. )*/
    int TLBcause = CauseExcCode(currentSupport->sup_exceptState[PGFAULTEXCEPT].s_cause);

    /* 3. If the Cause is a TLB-Modification exception, treat this exception as a program trap [Section 4.8] */
    /* from POPS Table 3.2, page 19 and from PANDOS 3.7.2 */
    if (TLBcause == 1){
        program_trap_handler();
    }

    /* 4. Gain mutual exclusion over the Swap Pool table. (SYS3 – P operation on the Swap Pool semaphore) */
    SYSCALL(3, &swapPoolSema4, 0, 0);
    
    /* 5. Determine the missing page number (denoted as p): found in the saved exception state’s EntryHi. */
    int missingVPN = (currentSupport->sup_exceptState[PGFAULTEXCEPT].s_entryHI >> 12) & 0x000FFFFF; 
    
    /* find page table index for later use */
    int pgTableIndex;
    if (missingVPN == 0xBFFFF) {
        pgTableIndex = 31;                              
    } else {
        pgTableIndex = (missingVPN - 0x80000);  
    }

    /* 6. Pick a frame, i, from the Swap Pool. Which frame is selected is determined by the Pandos page replacement algorithm. [Section 4.5.4]*/
    int pickedFrame = page_replace();

    /* 7. Determine if frame i is occupied; examine entry i in the Swap Pool table. */
    /* POPS 6.3.2 */
    if ((swapPoolTable[pickedFrame].ASID != -1)){ 
        /* disable interrupts */
        setSTATUS(getSTATUS() & (~IECBITON));
        /* (a) Update process x’s Page Table: mark Page Table entry k as not valid. This entry is easily accessible, since the Swap Pool table’s entry i contains a pointer to this Page Table entry. */
        pte_t *occupiedPgTable = swapPoolTable[pickedFrame].matchingPgTableEntry;
        occupiedPgTable->EntryLo = (0x00000400 & 0xFFFFFEFF) & 0xFFFFFDFF; 
        /* (b) Update the TLB, if needed. */
        TLBCLR();
        int write_out_pg_tbl;
        if (swapPoolTable[pickedFrame].pgNo == 0xBFFFF) {
            write_out_pg_tbl = 31;                              
        } else {
            write_out_pg_tbl = (swapPoolTable[pickedFrame].pgNo - 0x80000);  
        }
        /* (c) Update process x’s backing store. [Section 4.5.1]
        Treat any error status from the write operation as a program trap. [Section 4.8]*/
        if ((occupiedPgTable->EntryLo & 0x00000400) == 0x400) {  /* D bit set */
            debugRefreshBackingStore();
            read_write_flash(pickedFrame, swapPoolTable[pickedFrame].ASID - 1, write_out_pg_tbl, 0);  /* isRead = 0 since we are writing */
        }
        /* enable interrupts */
        setSTATUS(getSTATUS() | IECBITON);
    }

    /* 9. Read the contents of the Current Process’s backingstore/flash device logical page p into frame i. [Section 4.5.1] */
    read_write_flash(pickedFrame, currentSupport->sup_asid - 1, pgTableIndex, 1);  /* isRead = 1 since we are reading */

    /* 10. Update the Swap Pool table’s entry i to reflect frame i’s new contents: page p belonging to the Current Process’s ASID, and a pointer to the Current Process’s Page Table entry for page p. */
    swapPoolTable[pickedFrame].ASID = currentSupport->sup_asid;
    swapPoolTable[pickedFrame].pgNo = missingVPN;
    swapPoolTable[pickedFrame].matchingPgTableEntry = &(currentSupport->sup_privatePgTbl[pgTableIndex]);

    setSTATUS(getSTATUS() & (~IECBITON));
    /* 11. Update the Current Process’s Page Table entry for page p to indicate it is now present (V bit) and occupying frame i (PFN field).*/
    /* Set new PFN */
    swapPoolTable[pickedFrame].matchingPgTableEntry->EntryLo = (0x20020000 + (pickedFrame * 4096));
    /* Set V bit */
    swapPoolTable[pickedFrame].matchingPgTableEntry->EntryLo |= 0x00000200;
    /* Set D bit */
    swapPoolTable[pickedFrame].matchingPgTableEntry->EntryLo |= 0x00000400;

    /* 12. Update the TLB. */
    TLBCLR();
    setSTATUS(getSTATUS() | IECBITON);

    /* 13. Release mutual exclusion over the Swap Pool table. SYS4 */
    SYSCALL(4, &swapPoolSema4, 0, 0);

    /* 14. Return control to the Current Process */
    LDST((state_PTR) &(currentSupport->sup_exceptState[PGFAULTEXCEPT]));
}


