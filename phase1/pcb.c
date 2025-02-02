/*********************************PCBS.C*******************************
 *
 *	Allocation and Deallocation of Pcbs (phase 1).
 *
 *	Include freePcb, allocPcb, initPcbs
 *
 *      Modified by Phuong and Oghap on Jan 2025
 */
#include "../h/pcb.h"
#define MAXPROC	20

HIDDEN    pcb_PTR *pcbFree_h;    
static pcb_PTR MAXPROC_pcbs[MAXPROC];

void freePcb (pcb_PTR *p){
    p->p_next = pcbFree_h;
    pcbFree_h = p;
}

pcb_PTR *allocPcb (){
    if (pcbFree_h == NULL){
        return NULL;
    }
    pcb_PTR *allocatedPcb = pcbFree_h;

    pcbFree_h = pcbFree_h->p_next;
    
    allocatedPcb->p_child = NULL;
    allocatedPcb->p_next = NULL;
    allocatedPcb->p_prev = NULL;
    allocatedPcb->p_prnt = NULL;
    allocatedPcb->p_s.s_entryHI = 0;
    allocatedPcb->p_s.s_cause = 0;
    allocatedPcb->p_s.s_status = 0;
    allocatedPcb->p_s.s_pc = 0; 
    allocatedPcb->p_s.s_reg[STATEREGNUM] = 0;
    allocatedPcb->p_time = 0;
    allocatedPcb->p_semAdd = NULL;

    return allocatedPcb;
}

void initPcbs (){
    pcbFree_h = &(MAXPROC_pcbs[0]);
    int i;
    for (i = 0; i < MAXPROC - 1; i = i+1){
       (MAXPROC_pcbs[i]).p_next = &(MAXPROC_pcbs[i+1]);
    }
    (MAXPROC_pcbs[i]).p_next = NULL;
}

pcb_PTR *mkEmptyProcQ (){
    static pcb_PTR *queueTail = NULL;
    return queueTail;
}

int emptyProcQ (pcb_PTR *tp){
    return tp == NULL;
}

void insertProcQ (pcb_PTR **tp, pcb_PTR *p){
    if (emptyProcQ(*tp)){
        (*tp) = p;
        p->p_next = p;
        p->p_prev = p;
        return;
    }
    p->p_next = (*(*tp)).p_next;
    p->p_prev = (*tp);
    (*tp)->p_next = p;
    (*tp)->p_next->p_prev = p;
    *tp = p;
}

pcb_PTR *removeProcQ (pcb_PTR **tp){
    if (emptyProcQ(*tp) == NULL){
        return NULL;
    }

    pcb_PTR *rm = (*tp)-> p_next;

    (*tp)->p_next = rm->p_next;
    rm->p_next->p_prev = (*tp);
    return rm;

}

/*
pcb_PTR *outProcQ (pcb_PTR **tp, pcb_PTR *p);
pcb_PTR *headProcQ (pcb_PTR *tp);*/
