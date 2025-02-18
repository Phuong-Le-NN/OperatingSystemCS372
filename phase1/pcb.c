/*********************************PCB.C*******************************
 *
 *	This is the implementation of the process queue and process tree module.
 *  The process queues are implemented as doubly, circular linked lists,
 *  with p_prev and p_next fields. And the process queues are pointed by
 *  a tail pointer instead of a head pointer. 
 * 
 *  The pcbs are also organized into trees of pcbs, called process trees. 
 *  The p_prnt, p_child, and p_sib pointers are used for this purpose.
 *  A parent pcb contains a pointer (p_child) to a single, doubly linearly linked 
 *  list of its child pcbs. Each child process has a pointer 
 *  to its parent pcb (p_prnt) and the next child pcb of its parent (p_sib).
 * 
 *      Modified by Phuong and Oghap on Feb 2025
 */
#include "../h/pcb.h"
#include "../h/types.h"

HIDDEN    pcb_PTR       pcbFree_h;    
static    pcb_t         MAXPROC_pcbs[MAXPROC];


/**********************************************************
 *  Insert the element pointed to by p onto the pcbFree list. 
 * 
 *  Parameters:
 *         pcb_PTR p - the element pointed to by p
 * 
 *  Returns:
 *         
 */
void freePcb (pcb_PTR p){

    /*adding p to the free pcbFree list*/
    p->p_next = pcbFree_h;
    pcbFree_h = p;
}


/**********************************************************
 * Return NULL if the pcbFree list is empty. Otherwise, remove
 * an element from the pcbFree list, provide initial values for ALL
 * of the pcbs fields (i.e. NULL and/or 0) and then return a pointer
 * to the removed element. pcbs get reused, so it is important that
 * no previous value persist in a pcb when it gets reallocated.
 * 
 *  Parameters:
 *         pcb_PTR p - the element pointed to by p
 * 
 *  Returns:
 *         
 */
pcb_PTR allocPcb (){

    /* Special Case - checking if pcbList empty */
    if (pcbFree_h == NULL){
        return NULL;
    }


    /* removing the first p in pcbFree_h */
    pcb_PTR allocatedPcb = pcbFree_h;

    pcbFree_h = pcbFree_h->p_next;
    
    /* Provide initial values */
    allocatedPcb->p_child = NULL;
    allocatedPcb->p_next = NULL;
    allocatedPcb->p_prev = NULL;
    allocatedPcb->p_prnt = NULL;
    allocatedPcb->p_next_sib = NULL;
    allocatedPcb->p_prev_sib = NULL;
    allocatedPcb->p_s.s_entryHI = 0;
    allocatedPcb->p_s.s_cause = 0;
    allocatedPcb->p_s.s_status = 0;
    allocatedPcb->p_s.s_pc = 0; 
    allocatedPcb->p_s.s_reg[STATEREGNUM] = 0;
    allocatedPcb->p_time = 0;
    allocatedPcb->p_semAdd = NULL;

    return allocatedPcb;
}

/**********************************************************
 * Initialize the pcbFree list to contain all the elements of the
 * static array of MAXPROC pcbs. This method will be called only
 * once during data structure initialization.
 * 
 *  Parameters:
 *         
 * 
 *  Returns:
 *         
 */
void initPcbs (){

    /* Start at the head */
    pcbFree_h = &(MAXPROC_pcbs[0]);

    /*  traverse through the static array so that the 
    *   pcbFree list contains all elements. */
    int i;
    for (i = 0; i < MAXPROC - 1; i = i+1){
        (MAXPROC_pcbs[i]).p_next = &(MAXPROC_pcbs[i+1]);
    }
    (MAXPROC_pcbs[i]).p_next = NULL;
}


/**********************************************************
 * This method is used to initialize a variable to be tail pointer to a
 * process queue. Return a pointer to the tail of an empty process queue; 
 * i.e. NULL.
 * 
 *  Parameters:
 *         
 * 
 *  Returns:
 *      pcb_PTR - returns a tail pointer
 *         
 */
pcb_PTR mkEmptyProcQ (){

    /* Initializes the pointer of the tail of an empty process queue */
    static pcb_PTR queueTail = NULL;
    return queueTail;
}


/**********************************************************
 * Return TRUE if the queue whose tail is pointed to by tp is empty.
 * Return FALSE otherwise.
 * 
 *  Parameters:
 *      pcb_PTR tp - the tail pointer of pq
 * 
 *  Returns:
 *      boolean value
 */
int emptyProcQ (pcb_PTR tp){

    /* sets the tail pointer to NULL */
    return tp == NULL;
}

/**********************************************************
 * Insert the pcb pointed to by p into the process queue whose tail-
 * pointer is pointed to by tp. Note the double indirection through tp
 * to allow for the possible updating of the tail pointer as well
 * 
 *  Parameters:
 *       pcb_PTR *tp - the address of the tail pointer of pq        
 *       pcb_PTR p - pointer to the element being inserted
 *  
 *  Returns:
 *         
 */
void insertProcQ (pcb_PTR *tp, pcb_PTR p){
    
    /* Special case - if pq is empty, make p the tail */
    if (emptyProcQ(*tp)){
        (*tp) = p;
        p->p_next = p;
        p->p_prev = p;
        return;
    }

    /* Insert element at the tail */
    p->p_next = (*tp)->p_next;
    p->p_prev = (*tp);
    ((*tp)->p_next)->p_prev = p;
    (*tp)->p_next = p;
    *tp = p;
}

/**********************************************************
 *  Remove the first (i.e. head) element from the process queue whose
 *  tail-pointer is pointed to by tp. Return NULL if the process queue
 *  was initially empty; otherwise return the pointer to the removed element.
 *  Update the process queue’s tail pointer if necessary.
 * 
 *  Parameters:
 *         pcb_PTR *tp - address of the tail pointer of pq
 * 
 *  Returns:
 *         pcb_PTR removed_p 
 * 
 */
pcb_PTR removeProcQ (pcb_PTR *tp){
    
    /* Special case - pq is empty */
    if (emptyProcQ(*tp)){
        return NULL;
    }

    pcb_PTR rm = (*tp)-> p_next;

    /* Removing head element from pq */
    (*tp)->p_next = rm->p_next;
    rm->p_next->p_prev = (*tp);

    /* Special case - if removing the last element in pq,
    *  make the tail pointer = NULL*/
    if (rm == (*tp)){
        (*tp) = NULL;
    }

    return rm;

}

/**********************************************************
 * Remove the pcb pointed to by p from the process queue whose tail-
 * pointer is pointed to by tp. Update the process queue’s tail pointer if
 * necessary. If the desired entry is not in the indicated queue (an error
 * condition), return NULL; otherwise, return p. Note that p can point
 * to any element of the process queue
 * 
 *  Parameters:
 *       pcb_PTR *tp   - the address of the tail pointer of pq        
 *       pcb_PTR p     - pointer to the element being removed
 *  
 *  Returns:
 *         pcb_PTR removed_p       
 */
pcb_PTR outProcQ (pcb_PTR *tp, pcb_PTR p){
    
    /* Special Case - when pq is empty */
    if ((*tp) == NULL){
        return NULL;
    }

    /* find the element p */
    pcb_PTR traverse = *tp;
    while ((traverse != p) && (traverse->p_next != (*tp))) {
        traverse = traverse->p_next;
    }

    /* Special Case - when p is not in the pq */
    if (traverse != p){
        return NULL;
    }

    /* remove pcb from the queue */
    pcb_PTR next = p->p_next;
    pcb_PTR prev = p->p_prev;

    (next)->p_prev = prev;
    (prev)->p_next = next;

    /* Special case when removed element is the tail */
    if ((*tp) == p){
        (*tp) = p->p_prev;
        if ((*tp) == p){
            (*tp) = NULL;
        }
    }

    /* setting removed pcb values to NULL */
    p->p_prev = NULL;
    p->p_next = NULL;

    return p;
}

/**********************************************************
 * Return a pointer to the first pcb from the process queue whose tail
 * is pointed to by tp. Do not remove this pcbfrom the process queue.
 * Return NULL if the process queue is empty
 * 
 *  Parameters:
 *         pcb_PTR tp - tail pointer
 * 
 *  Returns:
 *         
 */
pcb_PTR headProcQ (pcb_PTR tp){
    
    /* Special Case - when pq is empty */
    if (tp == NULL){
        return NULL;
    }

    /* Return Head */
    return tp->p_next;

}

/**********************************************************
 * Return TRUE if the pcb pointed to by p has no children. Return
 * FALSE otherwise.
 * 
 *  Parameters:
 *         pcb_PTR p - element in pq
 * 
 *  Returns:
 *         boolean value - TRUE if the pcb pointed to by p has no children. 
 *                         Return FALSE otherwise.
 *         
 */
int emptyChild (pcb_PTR p){

    /* returning boolean value of whether the pcb pointed to by p has no children */
    return p->p_child == NULL;
}

/**********************************************************
 * Make the pcb pointed to by p a child of the pcb pointed to by prnt.
 * 
 *  Parameters:
 *         pcb_PTR prnt - pointer to the parent
 *         pcb_PTR p - element in pq
 * 
 *  Returns:
 *         
 */
void insertChild (pcb_PTR prnt, pcb_PTR p){

    /* Insert child using insert method from the process queue module */
    insertProcQ(&(prnt->p_child), p);
    p->p_prnt = prnt;
}

/**********************************************************
 * Make the first child of the pcb pointed to by p no longer a child of
 * p. Return NULL if initially there were no children of p. Otherwise,
 * return a pointer to this removed first child pcb.
 *  
 * Parameters:
 *         pcb_PTR p - pointer to element in pq
 * 
 *  Returns:
 *         pcb_PTR p - pointer to the removed child in pq
 *         
 */
pcb_PTR removeChild (pcb_PTR p){

    /*remove first child of the pcb from the queue using removeProcQ() from pcb module*/
    return removeProcQ(&(p->p_child));
}


/**********************************************************
 * Make the pcb pointed to by p no longer the child of its parent. If
 * the pcb pointed to by p has no parent, return NULL; otherwise, return
 * p. Note that the element pointed to by p need not be the first child of
 * its parent.
 * 
 * Parameters:
 *         pcb_PTR p - pointer to element in pq
 * 
 *  Returns:
 *         pcb_PTR p - pointer to the removed child in pq
 *         
 */
pcb_PTR outChild (pcb_PTR p){

    pcb_PTR prnt = p->p_prnt;

    /* special case when the the pcb pointed to by p has no parent */
    if (prnt == NULL){
        return NULL;
    }

    /*return the remove child of the pcb from the queue using outProcQ() from pcb module*/
    return outProcQ(&(prnt->p_child), p);
}
