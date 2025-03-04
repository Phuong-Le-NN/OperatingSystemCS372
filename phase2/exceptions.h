/************************** EXCEPTIONS.H ******************************
*
*
*  Written by Phuong and Oghap on Feb 2025
*/

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

extern void uTLB_RefillHandler ();

void CREATEPROCESS();
void TERMINATEPROCESS();
void PASSEREN();
pcb_PTR VERHOGEN();
void WAITIO();
void GETCPUTIME();
void WAITCLOCK();
void GETSUPPORTPTR();
unsigned int SYSCALL_handler();

void pass_up_or_die(int exception_constant);
void exception_handler();

#endif