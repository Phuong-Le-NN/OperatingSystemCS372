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

/*#include "/usr/include/umps3/umps/libumps.h"*/

#include "vmSupport.h"

/**********************************************************
 *  
 **********************************************************/
void initSwapStruct(){/* 4.1 -- Address Translation */
    /*
    initSwapStructs which will do the work of initializing both the Swap Pool table and accompanying semaphore [4.11.2 Module Decomposition]
    */

    /* One Page Table per U-proc ..  This array should be added to the Support Structure (support t) that is pointed to by a Uproc’s pcb => added pte_t table in support_t struct in types.h*/
    swapPoolFrame_t swapPoolTable[8 * 2]; /* The size of the Swap Pool should be set to two times UPROCMAX, where UPROCMAX is defined as the specific degree of multiprogramming to be supported: [1. . .8]*/
    
    /* Pandos Section 4.4.1 (page 48–49) */
    int i;
    for (i = 0; i < 16; i++) {
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

    int missingVPN_idx_in_pgTable = (missingVPN - 0x80000)/0x1000;

    /* if missing page is the stack page, finding the page entry indx in the page table like that would not work*/
    if (missingVPN == 0xBFFFF){
        missingVPN_idx_in_pgTable = 31;
    }

    support_t *currentSupport = SYSCALL(8, 0, 0, 0);
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
    for (i = 0; i < (2 * 8); i = i + 1){
        if (swapPoolTable[i].ASID == -1) {  /* from PANDOS 4.4.1 Technical Point */
            if (i == nextFrame){ /* so that frame i doesn't get replace right away next time but only after cirulated*/
                nextFrame = (nextFrame + 1) % (2 * 8);
            }
            return i;
        }
    }
    
    /* If no free frame, select the oldest one (FIFO) */ 
    int selectedFrame = nextFrame;
    nextFrame = (nextFrame + 1) % (2 * 8);  /* Move to next in circular order */

    return selectedFrame;
}

/**********************************************************
 * flash I/O function: read or write a page
 **********************************************************/
void read_write_flash(int pickedSwapPoolFrame, int isRead) {
    support_t *currentSupport = SYSCALL(8, 0, 0, 0);

    int devNo = currentSupport->sup_asid - 1;
    int blockNo = swapPoolTable[pickedSwapPoolFrame].pgNo;
    int flashSemIdx = devSemIdx(FLASHINT, devNo, isRead)

    /* Get the device register address for the U-proc’s flash device */
    device_t *flashDevRegAdd = devAddrBase(FLASHINT, devNo);

    SYSCALL(3, &(mutex[flashSemIdx]), 0, 0);

    /* Write the physical memory address (start of frame) to DATA0 */ /* swap pool starts at 0x20020000 - pandos pg 48*/
    flashDevRegAdd->d_data0 = (memaddr)(0x20020000 + (pickedSwapPoolFrame * 4096));

    /* Choose the correct flash command */
    int flashCommand;
    if (isRead == 0) {
        flashCommand = 3;  /* FLASHWRITE */
    } else {
        flashCommand = 2;  /* FLASHREAD */
    }

    /* Disable interrupts to ensure to do atomically */
    setSTATUS(getSTATUS() & (~IECBITON));
    /* Write the command to COMMAND register */
    flashDevRegAdd->d_command = (blockNo << 8) | flashCommand;
    /* Block the process until the flash operation is complete */
    SYSCALL(5, FLASHINT, devNo, 0);
    /* Re-enable interrupts */
    setSTATUS(getSTATUS() | IECBITON);

    SYSCALL(4, &(mutex[flashSemIdx]), 0, 0);
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
        /* PANDOS 4.8 */
        program_trap_handler();
    }

    /* 4. Gain mutual exclusion over the Swap Pool table. (SYS3 – P operation on the Swap Pool semaphore) */
    SYSCALL(3, &swapPoolSema4, 0, 0);
    
    /* 5. Determine the missing page number (denoted as p): found in the saved exception state’s EntryHi. */
    int missingVPN = (((state_PTR) BIOSDATAPAGE)->s_entryHI >> 12) & 0x000FFFFF; 

    /* 6. Pick a frame, i, from the Swap Pool. Which frame is selected is determined by the Pandos page replacement algorithm. [Section 4.5.4]*/
    int pickedFrame = page_replace();

    /* 7. Determine if frame i is occupied; examine entry i in the Swap Pool table. */
    /* POPS 6.3.2 */
    if (swapPoolTable[pickedFrame].matchingPgTableEntry->EntryLo & 0x00000200 == 1){ 
        
        /* disable interrupts */
        setSTATUS(getSTATUS() & (~IECBITON));
 
        /* (a) Update process x’s Page Table: mark Page Table entry k as not valid. This entry is easily accessible, since the Swap Pool table’s entry i contains a pointer to this Page Table entry. */
        pte_t *occupiedPgTable = swapPoolTable[pickedFrame].matchingPgTableEntry;
        occupiedPgTable->EntryLo &= ~0x00000200; 

        /* (b) Update the TLB, if needed. */
        setENTRYHI(occupiedPgTable->EntryHi);
        TLBP();

        /* Extract the index value from the CP0 INDEX register */
        int index = getINDEX();

        /* Check if the entry is present in the TLB (Index.P == 0) */
         /* POPS 6.4 */
        if ((index & 0x80000000) == 0) {                           
            setENTRYLO(occupiedPgTable->EntryLo);
            TLBWI();
        }

        /* enable interrupts */
        /*  Pandos 4.5.3, 4.4.2, and POPS 6.4.*/
        setSTATUS(getSTATUS() | IECBITON);

        /* (c) Update process x’s backing store. [Section 4.5.1]
        Treat any error status from the write operation as a program trap. [Section 4.8]*/
        if (occupiedPgTable->EntryLo & 0x00000800) {  /* D bit set */
            read_write_flash(pickedFrame, 0);  /* isRead = 0 since we are writing */
        }
    }

    /* 9. Read the contents of the Current Process’s backingstore/flash device logical page p into frame i. [Section 4.5.1] */
    read_write_flash(pickedFrame, 1);  /* isRead = 1 since we are reading */

    /* 10. Update the Swap Pool table’s entry i to reflect frame i’s new contents: page p belonging to the Current Process’s ASID, and a pointer to the Current Process’s Page Table entry for page p. */
    swapPoolTable[pickedFrame].ASID = currentSupport->sup_asid;
    swapPoolTable[pickedFrame].pgNo = missingVPN;
    int pgTableIndex;

    if (missingVPN == 0xBFFFF) {
        pgTableIndex = 31;                              
    } else {
        pgTableIndex = (missingVPN - 0x80000) / 0x1;  
    }

    swapPoolTable[pickedFrame].matchingPgTableEntry = &(currentSupport->sup_privatePgTbl[pgTableIndex]);

    /* 11. Update the Current Process’s Page Table entry for page p to indicate it is now present (V bit) and occupying frame i (PFN field).*/
    pte_t *newEntry = swapPoolTable[pickedFrame].matchingPgTableEntry;
    /* Set V bit */
    newEntry->EntryLo |= 0x00000200; 
     /* Clear old PFN */
    newEntry->EntryLo &= ~0xFFFFF000;
    /* Set new PFN */
    newEntry->EntryLo |= (pickedFrame << 12);

    /* 12. Update the TLB. */
    setSTATUS(getSTATUS() & (~IECBITON));
    setENTRYHI(newEntry->EntryHi);
    setENTRYLO(newEntry->EntryLo);
    TLBWR();
    setSTATUS(getSTATUS() | IECBITON);

    /* 13. Release mutual exclusion over the Swap Pool table. SYS4 */
    SYSCALL(4, &swapPoolSema4, 0, 0);

    /* 14. Return control to the Current Process */
    LDST((state_t *) &(currentSupport->sup_exceptState[PGFAULTEXCEPT]));
}


