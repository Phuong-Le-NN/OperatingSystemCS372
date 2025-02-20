/*********************************EXCEPTIONS.C*******************************
 *
 * 
 *      Modified by Phuong and Oghap on Feb 2025
 */


#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"

void uTLB_RefillHandler () {
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST ((state_PTR) 0x0FFFF000);
}

void fooBar(){
    
}

int CREATEPROCESS(){

    return;
}

int TERMINATEPROCESS(){

    return;
}

int PASSEREN(){

    return;
}

int VERHOGEN(){

    return;
}

int WAITIO(){

    return;
}

int GETCPUTIME(){

    return;
}

int WAITCLOCK(){

    return;
}

int GETSUPPORTPTR(){

    return;
}


int SYSCALL(int syscall,state_t *statep, support_t * supportp, 0) {

    return;

}
