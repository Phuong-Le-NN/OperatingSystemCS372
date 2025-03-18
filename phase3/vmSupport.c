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

/**********************************************************
 *  uTLB_RefillHandler()
 *  functions for reading and writing flash devices
 *  module-wide declared Swap Pool table and Swap Pool semaphore in initSwapStructs() function
 **********************************************************/

void initSwapStruct(){/* 4.1 -- Address Translation: The OS Perspective & 4.4.1 The Swap Pool */
    /*
    initSwapStructs which will do the work of initializing both the Swap Pool table and accompanying semaphore [4.11.2 Module Decomposition]
    */

    /* One Page Table per U-proc ..  This array should be added to the Support Structure (support t) that is pointed to by a Uproc’s pcb => added pte_t table in support_t struct in types.h*/
    swapPoolFrame_t swapPoolTable[8 * 2]; /* The size of the Swap Pool should be set to two times UPROCMAX, where UPROCMAX is defined as the specific degree of multiprogramming to be supported: [1. . .8]*/
    /* QUESTION: how to "place  place the Swap Pool after the end of the operating system code ... Swap Pool’s starting address is: 0x2002.0000" [4.4.1]  */
    int swapPoolSema4 = 1; /* A mutual exclusion semaphore (hence initialized to 1) that controls access to the Swap Pool data structure.*/
    /* Backing store: None ? -- this basic version of the Support Level will use each U-proc’s flash device as its backing store device.*/
}

void initialize_Uproc_pgTable() { /* 4.2.1 Pandos - A U-proc’s Page Table*/
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

void initialize_Uproc_BackingStore () { /* 4.2.2 -- A U-proc’s Backing Store*/
    /* each U-proc will be associated with a unique flash device */
    /*?*/
}

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

void TLB_exception_handler() { /* 4.4.2 The Pager */
    
}


