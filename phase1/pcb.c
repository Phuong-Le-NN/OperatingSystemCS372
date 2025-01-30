/*********************************PCBS.C*******************************
 *
 *	Allocation and Deallocation of Pcbs (phase 1).
 *
 *	Include freePcb, allocPcb, initPcbs
 *
 *      Modified by Phuong and Oghap on Jan 2025
 */
#include "../h/pcb.h"

HIDDEN    pcb_PTR *pcbFree_h;


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
    static pcb_PTR MAXPROC_pcbs[20];
    pcbFree_h = &(MAXPROC_pcbs[0]);
    pcb_PTR *ptr = pcbFree_h;
    int i;
    for (i = 1; i < 20; i = i+1){
        ptr->p_next = &(MAXPROC_pcbs[i]);
        ptr = ptr->p_next;
    }
}