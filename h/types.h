#ifndef TYPES
#define TYPES

/****************************************************************************
 *
 * This header file contains utility types definitions.
 *
 ****************************************************************************/

#include "../h/const.h"

typedef signed int cpu_t;

typedef unsigned int memaddr;

/**********************************************************************************************
 * BIOS related structs
 */

/* Device Register */
typedef struct {
	unsigned int d_status;
	unsigned int d_command;
	unsigned int d_data0;
	unsigned int d_data1;
} device_t;

#define t_recv_status d_status
#define t_recv_command d_command
#define t_transm_status d_data0
#define t_transm_command d_data1

/* Device register type for terminals */
typedef struct termreg {
	unsigned int recv_status;
	unsigned int recv_command;
	unsigned int transm_status;
	unsigned int transm_command;
} termreg_t;
typedef union devreg {
	device_t dtp;
	termreg_t term;
} devreg_t;

/* Bus register area */
typedef struct devregarea {
	unsigned int rambase;
	unsigned int ramsize;
	unsigned int execbase;
	unsigned int execsize;
	unsigned int bootbase;
	unsigned int bootsize;
	unsigned int todhi;
	unsigned int todlo;
	unsigned int intervaltimer;
	unsigned int timescale;
	unsigned int TLBFloorAddr;
	unsigned int inst_dev[5];
	unsigned int interrupt_dev[5];
	devreg_t devreg[5][8];
} devregarea_t;

/* Pass Up Vector */
typedef struct passupvector {
	unsigned int tlb_refll_handler;
	unsigned int tlb_refll_stackPtr;
	unsigned int exception_handler;
	unsigned int exception_stackPtr;
} passupvector_t;

/********************************************************************************************
 * phase 3 structs
 */

typedef struct pte_t {
	unsigned int EntryHi; /* VPN (Virtual Page Number) and ASID*/
	unsigned int EntryLo; /* PFN (Physical Frame Number) and Valid/Dirty bits */
} pte_t;

typedef struct swapPoolFrame_t {
	int ASID;                    /* The ASID of the U-proc whose page is occupying the frame*/
	int pgNo;                    /* The logical page number (VPN) of the occupying page.*/
	pte_t *matchingPgTableEntry; /* A pointer to the matching Page Table entry in the Page Table belonging to the owner process. (i.e. ASID)*/
} swapPoolFrame_t;

/**********************************************************************************************
 * pcb related structs
 */

#define STATEREGNUM 31
typedef struct state_t {
	unsigned int s_entryHI;
	unsigned int s_cause;
	unsigned int s_status;
	unsigned int s_pc;
	int s_reg[STATEREGNUM];

} state_t, *state_PTR;

/* process context */
typedef struct context_t {
	/* process context fields */
	unsigned int c_stackPtr, /* stack pointer value */
	  c_status,              /* status reg value */
	  c_pc;                  /* PC address*/
} context_t;

typedef struct support_t {
	int sup_asid;                   /* Process Id (asid) */
	state_t sup_exceptState[2];     /* stored excpt states */
	context_t sup_exceptContext[2]; /* pass up contexts */
	pte_t sup_privatePgTbl[32];     /* A Pandos Page Table will be an array of 32 Page Table entries _ pg 43 pandos */
	int sup_stackTlb[500];          /* 2Kb area for the stack area for the process TLB exception handler*/
	int sup_stackGen[500];          /* 2Kb area for the stack area for the process's Support Level general exception handler*/

	int delaySem; /* delay facility for phase 5*/
} support_t;

/********************************************************************************************
 * phase 5 structs
 */

typedef struct delayd_t {
	struct delayd_t *d_next;
	int d_wakeTime;	/* the time of day when the U-proc should be woken */
	support_t *d_supStruct;
} delayd_t;

typedef struct pcb_t {
	/* process queue fields */
	struct pcb_t *p_next; /* ptr to next entry */
	struct pcb_t *p_prev; /* ptr to previous entry */
	                      /* process tree fields */
	struct pcb_t *p_prnt, /* ptr to parent */
	  *p_child,           /* ptr to 1st child */
	  *p_next_sib,        /* ptr to next sibling */
	  *p_prev_sib;        /* ptr to prev. sibling */
	                      /* process status information */
	state_t p_s;          /* processor state */
	cpu_t p_time;         /* cpu time used by proc */
	int *p_semAdd;        /* ptr to semaphore on */
	                      /* which proc is blocked */
	                      /* support layer information */
	support_t *p_supportStruct;
} pcb_t, *pcb_PTR;

/********************************************************************************************
 * phase 1 structs
 */

/* semaphore descriptor type */
typedef struct semd_t {
	struct semd_t *s_next; /* next element on the ASL */
	int *s_semAdd;         /* pointer to the semaphore*/
	pcb_PTR s_procQ;       /* tail pointer to a */
	                       /* process queue */
} semd_t;

#define s_at s_reg[0]
#define s_v0 s_reg[1]
#define s_v1 s_reg[2]
#define s_a0 s_reg[3]
#define s_a1 s_reg[4]
#define s_a2 s_reg[5]
#define s_a3 s_reg[6]
#define s_t0 s_reg[7]
#define s_t1 s_reg[8]
#define s_t2 s_reg[9]
#define s_t3 s_reg[10]
#define s_t4 s_reg[11]
#define s_t5 s_reg[12]
#define s_t6 s_reg[13]
#define s_t7 s_reg[14]
#define s_s0 s_reg[15]
#define s_s1 s_reg[16]
#define s_s2 s_reg[17]
#define s_s3 s_reg[18]
#define s_s4 s_reg[19]
#define s_s5 s_reg[20]
#define s_s6 s_reg[21]
#define s_s7 s_reg[22]
#define s_t8 s_reg[23]
#define s_t9 s_reg[24]
#define s_gp s_reg[25]
#define s_sp s_reg[26]
#define s_fp s_reg[27]
#define s_ra s_reg[28]
#define s_HI s_reg[29]
#define s_LO s_reg[30]

#endif
