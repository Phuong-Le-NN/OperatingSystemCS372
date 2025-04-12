
/* concatenates two strings together and prints them out */

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"


void main() {
	char buf[2];
	
	SYSCALL(READTERMINAL, (int)&buf[0], 0, 0);

    SYSCALL(WRITETERMINAL, (int)&buf[0], 1, 0);
	
	/* Terminate normally */	
	SYSCALL(TERMINATE, 0, 0, 0);
}

