/*********************************ASL.C*******************************
 *  Implementation of the Active Semaphore List
 *      Maintains a sorted NULL-terminated single, linked
 *      lists of semaphore descriptors pointed to by pointer semd_h
 *      Also maintains the semdFree list, to hold the unused semaphore descriptors.
 *      For greater ASL traversal efficiency, we place 
 *      a dummy node at both the head (s semAdd ← 0) and 
 *      tail (s semAdd ← MAXINT) of the ASL
 *      This module includes public functions to support initializing ASL, 
 *      insert into queue of a sema4 in ASL using insertBlockec(), 
 *      removing from head queue of a sema4 in ASL using removeBlocked(), 
 *      removing from anywhere in queue of a a sema4 in ASL outBlocked(),
 *      accessing head of a queue of a sema4 in ASL headBlocked().
 *      Some helper functions (not accessible publicly) are also implemented in this file.
 *      Modified by Phuong and Oghap on Feb 2025
 */


#include "../h/pcb.h"
#define MAXPROC	20

HIDDEN semd_t *semdFree_h;
static semd_t semdTable[MAXPROC+2];
static semd_t *semd_h = NULL;

/**********************************************************
 *  Cleaning the sema4 before adding to the semdFree_h
 * 
 *  Parameters:
 *         semd_t *p : pointer to the sema4 to be free
 * 
 *  Returns:
 *         
 *         
 */
void freeSemd (semd_t *p){
    /*set sema4 queue pointer to NULL, assuming there is no pcb inside it*/
    p->s_procQ = NULL;
    /*adding sema4 to the free sema4 list*/
    p->s_next = semdFree_h;
    semdFree_h = p;
}

/**********************************************************
 *  Initializing the sema4 before after removing from the semdFree_h list
 * 
 *  Parameters:
 *         int* semAdd :  the address to initialize for the new sema4
 * 
 *  Returns:
 *         semd_t *allocatedSemd : pointer to the allocated sema4
 *         
 */
semd_t *allocSemd (int* semAdd){
    /*special case no free sema4 to allocate*/
    if (semdFree_h == NULL){
        return NULL;
    }
    /*removing the first sema4 in semdFree_h*/
    semd_t *allocatedSemd = semdFree_h;

    semdFree_h = semdFree_h->s_next;
    
    /*initialize its attributes*/
    allocatedSemd->s_next = NULL;
    allocatedSemd->s_semAdd = semAdd;
    allocatedSemd->s_procQ = mkEmptyProcQ();

    return allocatedSemd;
}

/**********************************************************
 *  Filling the semaFree_h list with sema4 from semdTable
 *  Adding 2 dummy nodes into the semd_h list using allocSemd()
 * 
 *  Parameters:
 *         
 * 
 *  Returns:
 *         
 *         
 */
void initASL (){
    /*adding the first sema4 from semdTable to the semdFree_h list*/
    semdFree_h = &(semdTable[0]);
    int i;
    /*adding and connecting the rest sema4 from semdTable to the semdFree_h list*/
    for (i = 0; i < MAXPROC + 1; i = i+1){
        (semdTable[i]).s_next = &(semdTable[i+1]);
    }
    /*making the s_next pointer of the tail node NULL*/
    (semdTable[i]).s_next = NULL;

    /*allocating and adding the two dummy node into the ASL*/
    semd_t *headDummy = allocSemd(0);
    semd_t *tailDummy = allocSemd(&(semdTable[MAXPROC + 1]));
    semd_h = tailDummy;
    headDummy->s_next = semd_h;
    semd_h = headDummy;
}

/**********************************************************
 *  A Helper Function: Traverse the ASL to look for the sema4 with largest address 
 *  that is smaller than the provided address of a sema4 (possible pointer)
 * 
 *  Parameters: 
 *         int* semAdd : the sema4 descriptor to look for
 *         
 * 
 *  Returns:
 *         semd_t *traverse : pointer to the possible predecessor sema4
 *         
 */
semd_t *traverseASL (int* semAdd){
    semd_t *traverse = semd_h;
    /*the loop that move traverse pointer until the the next sema4 no longer has the descriptor smaller than the given sema4 descriptor*/
    while (traverse->s_next->s_semAdd < semAdd){
        traverse = traverse->s_next;
    }
    return traverse;
}

/**********************************************************
 *  Insert a provided pcb to a provided sema4
 * 
 *  Parameters: 
 *         int* semAdd : the sema4 descriptor to look for
 *         pcb_PTR *p : pointer to the pcb to add to the sema4 
 *         
 * 
 *  Returns:
 *         TRUE if fail to insert due to unfound and unable to allocate sema4 with descriptor semAdd
 *         FALSE otherwise 
 *         
 */
int insertBlocked (int *semAdd, pcb_PTR *p){
    /*look for the predecessor*/
    semd_t *predecessor = traverseASL(semAdd);
    /*special case where the given sema4 descriptor does not exist in ASL*/
    if (predecessor->s_next->s_semAdd != semAdd){
        semd_t *newSem = allocSemd(semAdd);
        if (newSem == NULL){
            return TRUE;
        }
        newSem->s_next = predecessor->s_next;
        predecessor->s_next = newSem;
    }
    /*insert into the queue of the found sema4 using insertProcQ from pcb module*/
    insertProcQ(&(predecessor->s_next->s_procQ), p);
    p->p_semAdd = semAdd;
    return FALSE;
}

/**********************************************************
 *  Removing from head queue of a sema4 in ASL and remove sema4 from ASL if no longer active
 * 
 *  Parameters: 
 *         int* semAdd : the sema4 descriptor to look for
 *         
 * 
 *  Returns:
 *         NULL if fail to remove
 *         pcb_PTR *resultPbc : pointer to the removed pcb
 *         
 */
pcb_PTR *removeBlocked (int *semAdd){
    /*look for the predecessor*/
    semd_t *predecessor = traverseASL(semAdd);
    /*special case where the given sema4 descriptor does not exist in ASL*/
    if (predecessor->s_next->s_semAdd != semAdd){
        return NULL;
    }
    /*remove the queue of the found sema4 using removeProcQ from pcb module*/
    pcb_PTR *resultPcb = removeProcQ(&(predecessor->s_next->s_procQ));
    /*special case where the sema4 found but empty*/
    if (resultPcb == NULL){
        return NULL;
    }
    /*fixing the pointer to the sema4 of pcb to NULL*/
    resultPcb->p_semAdd = NULL;
    /*removing sema4 from ASL if no longer active*/
    if (emptyProcQ(predecessor->s_next->s_procQ)){
        semd_t *toBeFreeSem = predecessor->s_next;
        predecessor->s_next = predecessor->s_next->s_next;
        freeSemd(toBeFreeSem);
    }
    return resultPcb;

}
/**********************************************************
 *  Removing a specified pcb from queue of a sema4 in ASL 
 * 
 *  Parameters: 
 *         pcb_PTR *p : the pcb to remove
 *         
 * 
 *  Returns:
 *         NULL if fail to remove
 *         pointer to the removed pcb
 *         
 */
pcb_PTR *outBlocked (pcb_PTR *p){
    int *semAdd = p->p_semAdd;
    /*look for the predecessor*/
    semd_t *predecessor = traverseASL(semAdd);
    /*special case where the given sema4 descriptor does not exist in ASL or its queue is empty*/
    if (predecessor->s_next->s_semAdd != semAdd || emptyProcQ(predecessor->s_next->s_procQ)){
        return NULL;
    }
    /*remove pcb from the queue of the found sema4 using outProcQ() from pcb module*/
    return outProcQ(&(predecessor->s_next->s_procQ), p);
}
/**********************************************************
 *  Accessing head of a queue of a sema4 in ASL
 * 
 *  Parameters: 
 *         int* semAdd : the sema4 descriptor to look for
 *         
 * 
 *  Returns:
 *         pointer to head of a queue
 *         
 */
pcb_PTR *headBlocked (int *semAdd){
    /*look for the predecessor*/
    semd_t *predecessor = traverseASL(semAdd);
    /*special case where the given sema4 descriptor does not exist in ASL or its queue is empty*/
    if (predecessor->s_next->s_semAdd != semAdd || emptyProcQ(predecessor->s_next->s_procQ)){
        return NULL;
    }
    /*return the head pcb of the queue of the specified*/
    return predecessor->s_next->s_procQ->p_next;
}

