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

/* global variables */

/* The Swap Pool must be a shared resource among all U-procs, meaning it should be globally accessible rather than local to a function. */

/* Section 4.4.1 - The Swap Pool
“The Support Level must maintain a table, one entry per frame in the Swap Pool, recording information about the logical page occupying it.”
“The Swap Pool table is a shared data structure that must be accessed or updated in a mutually exclusive manner.” */

/* Section 4.11.2 - Module Decomposition
“Instead of declaring them globally in initProc.c, they can be declared module-wide in vmSupport.c. 
The test function will now invoke a new ‘public’ function initSwapStructs, which will do the work of initializing both the Swap Pool table and accompanying semaphore.” */
HIDDEN swapPoolFrame_t swapPoolTable[8 * 2];
HIDDEN int swapPoolSema4;

/**********************************************************
 *  
 **********************************************************/
void initSwapStruct(){/* 4.1 -- Address Translation: The OS Perspective & 4.4.1 The Swap Pool */
    /*
    initSwapStructs which will do the work of initializing both the Swap Pool table and accompanying semaphore [4.11.2 Module Decomposition]
    */

    /* One Page Table per U-proc ..  This array should be added to the Support Structure (support t) that is pointed to by a Uproc’s pcb => added pte_t table in support_t struct in types.h*/
    swapPoolFrame_t swapPoolTable[8 * 2]; /* The size of the Swap Pool should be set to two times UPROCMAX, where UPROCMAX is defined as the specific degree of multiprogramming to be supported: [1. . .8]*/
    /* QUESTION: how to "place  place the Swap Pool after the end of the operating system code ... Swap Pool’s starting address is: 0x2002.0000" [4.4.1]  */
    /* int swapPoolSema4 = 1;*/ /* A mutual exclusion semaphore (hence initialized to 1) that controls access to the Swap Pool data structure.*/
    /* Backing store: None ? -- this basic version of the Support Level will use each U-proc’s flash device as its backing store device.*/

    /* Pandos Section 4.4.1 (page 48–49) */
    for (int i = 0; i < 16; i++) {
        swapPoolTable[i].ASID = -1;
        swapPoolTable[i].pgNo = -1;
        swapPoolTable[i].matchingPgTableEntry = NULL;
    }
    swapPoolSema4 = 1;

}
/**********************************************************
 *  
 **********************************************************/
void init_Uproc_pgTable() { /* 4.2.1 Pandos - A U-proc’s Page Table*/
    /* To initialize a Page Table one needs to set the VPN, ASID, V, and D bit fields for each Page Table entry*/
    int i;
    for (i = 0; i < 32; i ++){
        /* VPN field will be set to [0x80000..0x8001E] for the first 31 entries => should set up to idx 30 only => will have to reset the idx 31 element after the loop*/
        /* ASID field, for any given Page Table, will all be set to the U-proc’s unique ID*/
        currentP->p_supportStruct->sup_pgTable[i].EntryHi =  0x80000000 + i*0x1000 + currentP->p_supportStruct->sup_asid << 6;
        /*  D bit field will be set to 1 (on) - bit 10 to 1*/
        currentP->p_supportStruct->sup_pgTable[i].EntryLo = currentP->p_supportStruct->sup_pgTable[i].EntryLo | 0x00000400;
        /* G bit field will be set to 0 (off) - bit 8 to 0*/
        currentP->p_supportStruct->sup_pgTable[i].EntryLo = currentP->p_supportStruct->sup_pgTable[i].EntryLo & 0xFFFFFEFF;
        /* V bit field will be set to 0 (off) - bit 9 to 0*/
        currentP->p_supportStruct->sup_pgTable[i].EntryLo = currentP->p_supportStruct->sup_pgTable[i].EntryLo & 0xFFFFFDFF;  
    }
    /* reset the idx 31 element -- VPN for the stack page (Page Table entry 31) should be set to 0xBFFFF*/
    currentP->p_supportStruct->sup_pgTable[i].EntryHi =  0xBFFFF000 + currentP->p_supportStruct->sup_asid;
}
/**********************************************************
 *  
 **********************************************************/
void initialize_Uproc_BackingStore () { /* 4.2.2 -- A U-proc’s Backing Store*/
    /* each U-proc will be associated with a unique flash device */

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

    pte_t pte = currentP->p_supportStruct->sup_pgTable[missingVPN_idx_in_pgTable];

    /* Write this Page Table entry into the TLB*/
    setENTRYHI(pte.EntryHi);
    setENTRYLO(pte.EntryLo);
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
void read_write_flash(int pickedSwapPoolFrame, int isWrite) {
    /* Disable interrupts to ensure to do atomically */
    setSTATUS(getSTATUS() & (~IECBITON));

    int asid = currentP->p_supportStruct->sup_asid;
    int vpn = swapPoolTable[pickedSwapPoolFrame].pgNo;

    /* Get the device register address for the U-proc’s flash device */
    device_t *flashDevRegAdd = (device_t *) devAddrBase(FLASHINT, asid - 1);

    /* Write the physical memory address (start of frame) to DATA0 */
    flashDevRegAdd->d_data0 = (memaddr)(0x20000000 + (pickedSwapPoolFrame * 4096));

    /* Choose the correct flash command */
    int flashCommand;
    if (isWrite == 1) {
        flashCommand = 3;  /* FLASHWRITE */
    } else {
        flashCommand = 2;  /* FLASHREAD */
    }

    /* Write the command to COMMAND register */
    flashDevRegAdd->d_command = (vpn << 8) | flashCommand;

    /* Block the process until the flash operation is complete */
    SYSCALL(5, FLASHINT, asid - 1, 0);

    /* Re-enable interrupts */
    setSTATUS(getSTATUS() | IECBITON);
}


/**********************************************************
 *  
 **********************************************************/
void TLB_exception_handler() { /* 4.4.2 The Pager, Page Fault */
    
    /* page 50 of pandos */
    /* 1. Obtain the pointer to the Current Process’s Support Structure: SYS8. */
    support_t *currentSupport = (support_t *) SYSCALL(8, 0, 0, 0);

    /* 2. Determine the cause of the TLB exception. 
    The saved exception state re- sponsible for this TLB exception should be found in the Current Process’s Support Structure for TLB exceptions. 
    (sup exceptState[0]’s Cause register)*/
    int TLBcause = CauseExcCode(currentSupport->sup_exceptState[0].s_cause);

    /* 3. If the Cause is a TLB-Modification exception, treat this exception as a program trap [Section 4.8], otherwise continue. */
    /* from POPS Table 3.2, page 19 and from PANDOS 3.7.2 */
    if (TLBcause == 1){
        /* PANDOS 4.8, we have not coded this yet, but we will have to declare it for the sysSupport.c */
        programTrapHandler();
    }

    /* 4. Gain mutual exclusion over the Swap Pool table. (SYS3 – P operation on the Swap Pool semaphore) */
    /* I am not sure if this is right...*/
    SYSCALL(3, &swapPoolSema4, 0, 0);
    
    /* 5. Determine the missing page number (denoted as p): found in the saved exception state’s EntryHi. */
    int missingVPN = (((state_PTR) BIOSDATAPAGE)->s_entryHI >> 12) & 0x000FFFFF; /* missingVPN is in range [0x80000 ... 0x8001E]*/

    /* 6. Pick a frame, i, from the Swap Pool. Which frame is selected is determined by the Pandos page replacement algorithm. [Section 4.5.4]*/
    int pickedFrame = page_replace();

    /* 7. Determine if frame i is occupied; examine entry i in the Swap Pool table. */
    /* If frame i is currently occupied, assume it is occupied by logical page number k belonging to process x (ASID) and 
    that it is “dirty” (i.e. been modi- fied): */
    /* POPS 6.3.2 */
    if (swapPoolTable[pickedFrame].matchingPgTableEntry->EntryLo & 0x00000200){     /* POPS 6.3.2 */

        /* (a) Update process x’s Page Table: mark Page Table entry k as not valid. This entry is easily accessible, since the Swap Pool table’s entry i contains a pointer to this Page Table entry. */
        pte_t *occupiedPgTable = swapPoolTable[pickedFrame].matchingPgTableEntry;
        occupiedPgTable->EntryLo &= ~0x00000200;     /* POPS 6.3.2 */

        /* disable interrupts */
        /* Interrupts should be disabled when modifying shared memory structures (like the Page Table and TLB) to prevent inconsistencies.*/
        setSTATUS(getSTATUS() & (~IECBITON));


        /* (b) Update the TLB, if needed. The TLB is a cache of the most recently executed process’s Page Table entries. 
        If process x’s page k’s Page Table entry is currently cached in the TLB it is clearly out of date; 
        it was just updated in the previous step.
        Important Point: This step and the previous step must be accomplished atomically. [Section 4.5.3] */
        setENTRYHI(occupiedPgTable->EntryHi);
        TLBP();

        /* Extract the index value from the CP0 INDEX register */
        int index = getINDEX();

        /* Check if the entry is present in the TLB (Index.P == 0) */
        /* The TLBP (TLB-Probe) command initiates a TLB search for a matching entry in the TLB that matches the current values in the EntryHi CP0 regis- ter. 
        If a matching entry is found in the TLB the corresponding index value is loaded into Index.
        TLB-Index and the Probe bit (Index.P) is set to 0. If no match is found, Index.P is set to 1. [Section 7.2.1]*/
        if ((index & 0x80000000) == 0) {  /* 0x80000000 is the "P" bit (bit 31) from POPS 6.4 */
            setENTRYLO(occupiedPgTable->EntryLo);
            TLBWI();
        }

        /* enable interrupts */
        /*  Pandos 4.5.3, 4.4.2, and POPS 6.4.*/
        /* Interrupts should be disabled when modifying shared memory structures (like the Page Table and TLB) to prevent inconsistencies.
        Interrupts should be enabled afterward to allow the system to continue handling device events and process scheduling. */
        setSTATUS(getSTATUS() | IECBITON);

        /* (c) Update process x’s backing store. Write the contents of frame i to the correct location on process x’s backing store/flash device. [Section 4.5.1]
        Treat any error status from the write operation as a program trap. [Section 4.8]*/
        if (occupiedPgTable->EntryLo & 0x00000800) {  /* D bit set */
            read_write_flash(pickedFrame, 1);  /* isWrite = 1 */
        }
    }

    /* 9. Read the contents of the Current Process’s backingstore/flash device logical page p into frame i. [Section 4.5.1]
        Treat any error status from the read operation as a program trap. [Section 4.8] */
    read_write_flash(pickedFrame, 0);  /* isWrite = 0 */

    /* 10. Update the Swap Pool table’s entry i to reflect frame i’s new contents: page p belonging to the Current Process’s ASID, and a pointer to the Current Process’s Page Table entry for page p. */
    swapPoolTable[pickedFrame].ASID = currentSupport->sup_asid;
    swapPoolTable[pickedFrame].pgNo = missingVPN;
    int pgTableIndex;

    if (missingVPN == 0xBFFFF) {
        pgTableIndex = 31;  /* stack page is always at index 31 */
    } else {
        pgTableIndex = (missingVPN - 0x80000) / 0x1000;  /* compute index for text/data */
    }

    swapPoolTable[pickedFrame].matchingPgTableEntry = &(currentSupport->sup_pgTable[pgTableIndex]);

    /* 11. Update the Current Process’s Page Table entry for page p to indicate it is now present (V bit) and occupying frame i (PFN field).*/
    pte_t *newEntry = swapPoolTable[pickedFrame].matchingPgTableEntry;
    newEntry->EntryLo |= 0x00000200;  /* Set V bit */
    newEntry->EntryLo &= ~0xFFFFF000;  /* Clear old PFN */
    newEntry->EntryLo |= (pickedFrame << 12);  /* Set new PFN */

    /* 12. Update the TLB. The cached entry in the TLB for the Current Process’s page p is clearly out of date; it was just updated in the previous step.
    Important Point: This step and the previous step must be accomplished atomically. [Section 4.5.3]*/
    setSTATUS(getSTATUS() & (~IECBITON));
    setENTRYHI(newEntry->EntryHi);
    setENTRYLO(newEntry->EntryLo);
    TLBWR();
    setSTATUS(getSTATUS() | IECBITON);

    /* 13. Release mutual exclusion over the Swap Pool table. (SYS4 – V operation on the Swap Pool semaphore) */
    SYSCALL(4, &swapPoolSema4, 0, 0);

    /* 14. Return control to the Current Process to retry the instruction that caused the page fault: LDST on the saved exception state. */
    LDST((state_t *) &(currentSupport->sup_exceptState[PGFAULTEXCEPT]));
}


