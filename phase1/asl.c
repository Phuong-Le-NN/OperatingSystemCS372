 /*********************************ASL.C*******************************
 *  Implementation of the Active Semaphore List
 *      Maintains a sorted NULL-terminated single, linked
 *      lists of semaphore descriptors.s
 *
 *      Modified by Phuong and Oghap on Feb 2025
 */


#include "../h/pcb.h"
#define MAXPROC	20

HIDDEN semd_t *semdFree_h;
static semd_t semdTable[MAXPROC+2];
static semd_t *semd_h = NULL;

void freeSemd (semd_t *p){
    p->s_procQ = NULL;
    p->s_next = semdFree_h;
    semdFree_h = p;
}

semd_t *allocSemd (int* semAdd){
    if (semdFree_h == NULL){
        return NULL;
    }
    semd_t *allocatedSemd = semdFree_h;

    semdFree_h = semdFree_h->s_next;
    
    allocatedSemd->s_next = NULL;
    allocatedSemd->s_semAdd = semAdd;
    allocatedSemd->s_procQ = mkEmptyProcQ();

    return allocatedSemd;
}

void initASL (){
    semdFree_h = &(semdTable[0]);
    int i;
    for (i = 0; i < MAXPROC + 1; i = i+1){
        (semdTable[i]).s_next = &(semdTable[i+1]);
    }
    (semdTable[i]).s_next = NULL;

    semd_t *headDummy = allocSemd(0);
    semd_t *tailDummy = allocSemd(&(semdTable[MAXPROC + 1]));
    semd_h = tailDummy;
    headDummy->s_next = semd_h;
    semd_h = headDummy;
}

semd_t *traverseASL (int* semAdd){
    semd_t *traverse = semd_h;
    while (traverse->s_next->s_semAdd < semAdd){
        traverse = traverse->s_next;
    }
    return traverse;
}

int insertBlocked (int *semAdd, pcb_PTR *p){
    semd_t *predecessor = traverseASL(semAdd);
    if (predecessor->s_next->s_semAdd != semAdd){
        semd_t *newSem = allocSemd(semAdd);
        if (newSem == NULL){
            return TRUE;
        }
        newSem->s_next = predecessor->s_next;
        predecessor->s_next = newSem;
    }
    insertProcQ(&(predecessor->s_next->s_procQ), p);
    p->p_semAdd = semAdd;
    return FALSE;
}

pcb_PTR *removeBlocked (int *semAdd){
    semd_t *predecessor = traverseASL(semAdd);
    if (predecessor->s_next->s_semAdd != semAdd){
        return NULL;
    }
    pcb_PTR *resultPcb = removeProcQ(&(predecessor->s_next->s_procQ));
    if (resultPcb == NULL){
        return NULL;
    }
    resultPcb->p_semAdd = NULL;
    if (emptyProcQ(predecessor->s_next->s_procQ)){
        semd_t *toBeFreeSem = predecessor->s_next;
        predecessor->s_next = predecessor->s_next->s_next;
        freeSemd(toBeFreeSem);
    }
    return resultPcb;

}
pcb_PTR *outBlocked (pcb_PTR *p){
    int *semAdd = p->p_semAdd;
    semd_t *predecessor = traverseASL(semAdd);
    if (predecessor->s_next->s_semAdd != semAdd || emptyProcQ(predecessor->s_next->s_procQ)){
        return NULL;
    }
    return outProcQ(&(predecessor->s_next->s_procQ), p);
}

pcb_PTR *headBlocked (int *semAdd){
    semd_t *predecessor = traverseASL(semAdd);
    if (predecessor->s_next->s_semAdd != semAdd || emptyProcQ(predecessor->s_next->s_procQ)){
        return NULL;
    }
    return predecessor->s_next->s_procQ->p_next;
}

