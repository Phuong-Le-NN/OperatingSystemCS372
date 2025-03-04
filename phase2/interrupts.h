/************************** INTERRUPTS.H ******************************
*
*
*  Written by Phuong and Oghap on Feb 2025
*/

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

HIDDEN pcb_PTR helper_unblock_process(int *semdAdd);
void interrupt_exception_handler();

#endif