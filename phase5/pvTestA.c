/*	Test Virtual P's and V's. pvTestA (consumer) and pvTestB (producer)
 *	exchange a message using the shared segment for synchronization
 *	and data transmission.
 */

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

int *hold = (int *)(SEG3);
int *empty = (int *)(SEG3 + 4);
int *full = (int *)(SEG3 + 8);
char *charbuff = (char *)(SEG3 + 12);

#define		MESSIZE	40

void main() {
	char msg[MESSIZE], *p;

	print(WRITETERMINAL, "pvATest starts\n");

	/* initialize shared memory */
	*hold = 0;
	*full = 0;
	*empty = 1;

	/* block until pvTestB has started up */
	SYSCALL(PSEMVIRT, (int) hold, 0, 0);
	
	print(WRITETERMINAL, "pvATest is free\n");

	/* receive characters from t4 */
	p = msg;
	do {
		SYSCALL(PSEMVIRT, (int) full, 0, 0);
		*p = *charbuff;
		print(WRITETERMINAL, "."); 
		SYSCALL(VSEMVIRT, (int) empty, 0, 0);
	} while (*p++ != '\0');
	
	/* print message received from t4 */
	print(WRITETERMINAL, "\n");
	print(WRITETERMINAL, msg);

	print(WRITETERMINAL, "pvATest completed\n");
	
	/* terminate normally */
	SYSCALL(TERMINATE, 0, 0, 0);
	
	print(WRITETERMINAL, "pvATest error: did not terminate\n");
	HALT();
}
