/*********************************EXCEPTIONS.C*******************************
 *
 * 
 *      Modified by Phuong and Oghap on Feb 2025
 */


#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"


void main() {

}

void uTLB_RefillHandler () {
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST ((state_PTR) 0x0FFFF000);
}