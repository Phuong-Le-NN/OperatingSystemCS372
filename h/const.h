#ifndef CONSTS
#define CONSTS

/**************************************************************************** 
 *
 * This header file contains utility constants & macro definitions.
 * 
 ****************************************************************************/

/* Hardware & software constants */
#define PAGESIZE		  4096			/* page size in bytes	*/
#define WORDLEN			  4				  /* word size in bytes	*/


/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR		0x10000000
#define RAMBASESIZE		0x10000004
#define TODLOADDR		  0x1000001C
#define INTERVALTMR		0x10000020	
#define TIMESCALEADDR	0x10000024


/* utility constants */
#define	TRUE			    1
#define	FALSE			    0
#define HIDDEN			  static
#define EOS				    '\0'

#define NULL 			    ((state_t *)0xFFFFFFFF)

/* high precedence interrupts */
#define INTERPROCESSORINT       0
#define PLTINT                  1
#define INTERVALTIMERINT        2


/* device interrupts */
#define DISKINT			  3
#define FLASHINT 		  4
#define NETWINT 		  5
#define PRNTINT 		  6
#define TERMINT			  7

#define DEVINTNUM		  5		    /* interrupt lines used by devices */
#define DEVPERINT		  8		    /* devices per interrupt line */
#define DEVREGLEN		  4		    /* device register field length in bytes, and regs per dev */	
#define DEVREGSIZE	  16 		    /* device register size in bytes */

/* device register field number for non-terminal devices */
#define STATUS			  0
#define COMMAND			  1
#define DATA0			    2
#define DATA1			    3

/* device register field number for terminal devices */
#define RECVSTATUS  	0
#define RECVCOMMAND 	1
#define TRANSTATUS  	2
#define TRANCOMMAND 	3

/* device common STATUS codes */
#define UNINSTALLED		0
#define READY			    1
#define BUSY			    3

/* device common COMMAND codes */
#define RESET			    0
#define ACK				    1

/* Memory related constants */
#define KSEG0           0x00000000
#define KSEG1           0x20000000
#define KSEG2           0x40000000
#define KUSEG           0x80000000
#define RAMSTART        0x20000000
#define BIOSDATAPAGE    0x0FFFF000
#define	PASSUPVECTOR	0x0FFFF900

/* Exceptions related constants */
#define	PGFAULTEXCEPT	    0
#define GENERALEXCEPT	    1

#define INT                 0       /* External Device Interrupt*/
#define MOD                 1       /* TLB-Modification Exception*/
#define TLBL                2       /* TLB Invalid Exception: on a Load instr. or instruction fetch*/
#define TLBS                3       /* TLB Invalid Exception: on a Store instr.*/
#define ADEL                4       /* Address Error Exception: on a Load or instruction fetch*/
#define ADES                5       /* Address Error Exception: on a Store instr.*/
#define IBE                 6       /* Bus Error Exception: on an instruction fetch*/
#define DEB                 7       /* Bus Error Exception: on a Load/Store data access*/
#define SYS                 8       /* Syscall Exception*/
#define BP                  9       /* Breakpoint Exception*/
#define RI                  10      /* Reserved Instruction Exception*/
#define CPU                 11      /* Coprocessor Unusable Exception*/
#define OV                  12      /* Arithmetic Overflow Exception*/

/* Cause register bit fields */
#define EXECCODEBITS        0x0000007C
#define IPBITS              0x00110000

/* Device register related addresses*/
#define INSTALLED_DEV_REG   0x1000002C
#define INT_DEV_REG         0x10000040

/* operations */
#define	MIN(A,B)		((A) < (B) ? A : B)
#define MAX(A,B)		((A) < (B) ? B : A)
#define	ALIGNED(A)		(((unsigned)A & 0x3) == 0)

/* Macro to load the Interval Timer */
#define LDIT(T)	((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) 

/* Macro to read the TOD clock */
#define STCK(T) ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))

/* Macro to enable interrupt on current enable bit*/
#define enable_IEc(current_status) (current_status | 0x00000001)

/* Macro to disable PLT bit*/
#define disable_TE(current_status) (current_status & 0xf7ffffff)

/* Macro to enable PLT bit*/
#define enable_TE(current_status) (current_status | 0x08000000)

/* Macro to enable previous setting of the status.IEc bit*/
#define enable_IEp(current_status) (current_status | 0x00000004)

/* Macro to enable previous setting of the status.KUc bit*/
#define kernel(current_status) (current_status & 0xfffffff7)

/* Macro to calculate starting address of the device’s device register*/
#define devAddrBase(intLineNo, devNo) 0x10000054 + ((intLineNo - 3) * 0x80) + (devNo * 0x10);

/* Macro to get the ExcCode given the Code register*/
#define CauseExcCode(Cause) EXECCODEBITS & Cause;

/* Macro to calculate address of Interrupt Device Bit Map of Interrupt line */
#define intDevBitMap(intLine) INT_DEV_REG + (0x04*intLine); /* wrong calculation off set by 3*/

/* Macro to calculate address of Installed Device Bit Map of Interrupt line */
#define installedDevBitMap(intLine) INSTALLED_DEV_REG + (0x04*intLine); /* delete not needed*/

/* Macro to calculate the device sema4 idx in the device sema4 array*/
#define devSemIdx(intLineNo, devNo, temRead) (intLineNo - 3) * 8 + temRead* 8 + devNo;

/* Maximum number of semaphore and pcb that can be allocated*/
#define MAXPROC	20
#define	MAXSEM	MAXPROC

#endif
