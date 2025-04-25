#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../phase3/initProc.h"

#define DISK_DMA_BUFFER_BASE_ADDR   0x20020000
#define FLASK_DMA_BUFFER_BASE_ADDR  0x20020000 + PAGESIZE*8

#define READBLK     3
#define WRITEBLK    4

#define SECTNUM_SHIFT   8

void helper_copy_block(int *src, int *dst){

}

void WRITE_TO_DISK(support_t *currentSupport){
    //mutex dev reg and hence DMA buffer (the dev mutex sem)
    //write to data 0 the address
    //disable interrupt
    //write to command
    //syscall 5
    //enable interrupt
    //dma buffer to logical address
    //release mutex
    state_PTR saved_gen_exc_state = &(currentSupport->sup_exceptState[GENERALEXCEPT]);
    int devNo = saved_gen_exc_state->s_a2;
    int disk_sem_idx = devSemIdx(DISKINT, devNo, FALSE);

    device_t *disk_dev_reg_addr = devAddrBase(DISKINT, devNo);

    SYSCALL(PASSERN, &mutex[disk_sem_idx], 0, 0);
        helper_copy_block(saved_gen_exc_state->s_a1, DISK_DMA_BUFFER_BASE_ADDR + PAGESIZE*devNo);
        disk_dev_reg_addr->d_data0 = DISK_DMA_BUFFER_BASE_ADDR + PAGESIZE*devNo;
            setSTATUS(getSTATUS() & (~IECBITON));
                disk_dev_reg_addr->d_command = (saved_gen_exc_state->s_a3 << SECTNUM_SHIFT) + WRITEBLK; /*not sure -- what to put for head num? why head num? 3d or 1d?*/
                int disk_status = SYSCALL(IOWAIT, &mutex[disk_sem_idx], 0, 0);
            setSTATUS(getSTATUS() | IECBITON);
    SYSCALL(VERHO, &mutex[disk_sem_idx], 0, 0);

    if (disk_status == READY){
        saved_gen_exc_state->s_v0 = disk_status;
    } else {
        saved_gen_exc_state->s_v0 = 0 - disk_status;
    }
}

void READ_FROM_DISK(support_t *currentSupport){
    //mutex dev reg and hence DMA buffer (the dev mutex sem)
    //write to data 0 the address
    //disable interrupt
    //write to command
    //syscall 5
    //enable interrupt
    //dma buffer to logical address
    //release mutex
    state_PTR saved_gen_exc_state = &(currentSupport->sup_exceptState[GENERALEXCEPT]);
    int devNo = saved_gen_exc_state->s_a2;
    int disk_sem_idx = devSemIdx(DISKINT, devNo, FALSE);

    device_t *disk_dev_reg_addr = devAddrBase(DISKINT, devNo);

    SYSCALL(PASSERN, &mutex[disk_sem_idx], 0, 0);
        disk_dev_reg_addr->d_data0 = DISK_DMA_BUFFER_BASE_ADDR + PAGESIZE*devNo;
            setSTATUS(getSTATUS() & (~IECBITON));
                disk_dev_reg_addr->d_command = (saved_gen_exc_state->s_a3 << SECTNUM_SHIFT) + READBLK; /*not sure -- what to put for head num? why head num? 3d or 1d?*/
                int disk_status = SYSCALL(IOWAIT, &mutex[disk_sem_idx], 0, 0);
            setSTATUS(getSTATUS() | IECBITON);
        helper_copy_block(DISK_DMA_BUFFER_BASE_ADDR + PAGESIZE*devNo, saved_gen_exc_state->s_a1);
    SYSCALL(VERHO, &mutex[disk_sem_idx], 0, 0);

    if (disk_status == READY){
        saved_gen_exc_state->s_v0 = disk_status;
    } else {
        saved_gen_exc_state->s_v0 = 0 - disk_status;
    }
}