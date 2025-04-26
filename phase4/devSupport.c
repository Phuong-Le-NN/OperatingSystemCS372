#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../phase3/initProc.h"
#include "../phase3/vmSupport.h"

#define DISK_DMA_BUFFER_BASE_ADDR   0x20020000
#define FLASK_DMA_BUFFER_BASE_ADDR  0x20020000 + PAGESIZE*8
#define BLOCKSIZE   0xFA0

#define READBLK_DSK     3
#define WRITEBLK_DSK    4
#define SEEKCYL     2

#define SECTNUM_SHIFT   8
#define CYLNUM_SHIFT    8
#define HEADNUM_SHIFT   16

#define READBLK_FLASH   2
#define WRITEBLK_FLASH  3

#define BLOCKNUM_SHIFT  8

void helper_copy_block(int *src, int *dst){
    int i;
    for (i = 0; i < (BLOCKSIZE/PAGESIZE); i++){
        *dst = *src;
        dst++; /*should increase by 4*/
        src++;
    }
}

void WRITE_TO_DISK(support_t *currentSupport){
    state_PTR saved_gen_exc_state = &(currentSupport->sup_exceptState[GENERALEXCEPT]);
    if (saved_gen_exc_state->s_a1 < KUSEG){
        program_trap_handler(currentSupport, NULL);
    }
    int devNo = saved_gen_exc_state->s_a2;
    int disk_sem_idx = devSemIdx(DISKINT, devNo, FALSE);

    device_t *disk_dev_reg_addr = devAddrBase(DISKINT, devNo);

    SYSCALL(PASSERN, &mutex[disk_sem_idx], 0, 0);
        helper_copy_block(saved_gen_exc_state->s_a1, DISK_DMA_BUFFER_BASE_ADDR + PAGESIZE*devNo);
        int maxcyl = (disk_dev_reg_addr->d_data1 >> 16) & 0xFFFF;
        int maxhead = (disk_dev_reg_addr->d_data1 >> 8) & 0xFF;
        int maxsect = disk_dev_reg_addr->d_data1 & 0xFF;
        int sectNo = saved_gen_exc_state->s_a3 % maxsect;
        int headNo = (saved_gen_exc_state->s_a3 / maxsect) % maxhead; /*divide and round down*/
        int cylNo = (saved_gen_exc_state->s_a3 / maxsect) % maxcyl;
        disk_dev_reg_addr->d_data0 = DISK_DMA_BUFFER_BASE_ADDR + PAGESIZE*devNo;
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (cylNo << CYLNUM_SHIFT) + SEEKCYL;
            int disk_status = SYSCALL(IOWAIT, &mutex[disk_sem_idx], 0, 0);
        setSTATUS(getSTATUS() | IECBITON);
        if (disk_status != READY){
            saved_gen_exc_state->s_v0 = -disk_status;
            SYSCALL(VERHO, &mutex[disk_sem_idx], 0, 0);
            return;
        }
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (headNo << HEADNUM_SHIFT) + (sectNo << SECTNUM_SHIFT) + WRITEBLK_DSK;
            disk_status = SYSCALL(IOWAIT, &mutex[disk_sem_idx], 0, 0);
        setSTATUS(getSTATUS() | IECBITON);
    SYSCALL(VERHO, &mutex[disk_sem_idx], 0, 0);

    if (disk_status == READY){
        saved_gen_exc_state->s_v0 = disk_status;
    } else {
        saved_gen_exc_state->s_v0 = 0 - disk_status;
    }
}

void READ_FROM_DISK(support_t *currentSupport){
    state_PTR saved_gen_exc_state = &(currentSupport->sup_exceptState[GENERALEXCEPT]);
    int devNo = saved_gen_exc_state->s_a2;
    int disk_sem_idx = devSemIdx(DISKINT, devNo, FALSE);

    device_t *disk_dev_reg_addr = devAddrBase(DISKINT, devNo);

    SYSCALL(PASSERN, &mutex[disk_sem_idx], 0, 0);
        int maxcyl = (disk_dev_reg_addr->d_data1 >> 16) & 0xFFFF;
        int maxhead = (disk_dev_reg_addr->d_data1 >> 8) & 0xFF;
        int maxsect = disk_dev_reg_addr->d_data1 & 0xFF;
        int sectNo = saved_gen_exc_state->s_a3 % maxsect;
        int headNo = (saved_gen_exc_state->s_a3 / maxsect) % maxhead; /*divide and round down*/
        int cylNo = (saved_gen_exc_state->s_a3 / maxsect) % maxcyl;
        disk_dev_reg_addr->d_data0 = DISK_DMA_BUFFER_BASE_ADDR + PAGESIZE*devNo;
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (cylNo << CYLNUM_SHIFT) + SEEKCYL;
            int disk_status = SYSCALL(IOWAIT, &(mutex[disk_sem_idx]), 0, 0);
        setSTATUS(getSTATUS() | IECBITON);
        if (disk_status != READY){
            saved_gen_exc_state->s_v0 = -disk_status;
            SYSCALL(VERHO, &mutex[disk_sem_idx], 0, 0);
            return;
        }
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (headNo << HEADNUM_SHIFT) + (sectNo << SECTNUM_SHIFT) + READBLK_DSK; /*not sure -- what to put for head num? why head num? 3d or 1d?*/
            disk_status = SYSCALL(IOWAIT, &(mutex[disk_sem_idx]), 0, 0);
        setSTATUS(getSTATUS() | IECBITON);
        helper_copy_block(DISK_DMA_BUFFER_BASE_ADDR + PAGESIZE*devNo, saved_gen_exc_state->s_a1);
    SYSCALL(VERHO, &mutex[disk_sem_idx], 0, 0);

    if (disk_status == READY){
        saved_gen_exc_state->s_v0 = disk_status;
    } else{
        saved_gen_exc_state->s_v0 = 0 - disk_status;
    }
}

void READ_FROM_FLASH(support_t *currentSupport){
    state_PTR saved_exception_state = &(currentSupport->sup_exceptState[GENERALEXCEPT]);
    int devNo = saved_exception_state->s_a2;
    int flash_sem_idx = devSemIdx(FLASHINT, devNo, FALSE);
    device_t *flash_dev_reg_addr = devAddrBase(FLASHINT, devNo);

    if ((saved_exception_state->s_a1 < KUSEG) || (saved_exception_state->s_a3 < 32) || (saved_exception_state->s_a3 >= flash_dev_reg_addr->d_data1)){
        program_trap_handler(currentSupport, NULL);
    }

    SYSCALL(PASSERN, &(mutex[flash_sem_idx]), 0, 0);
        flash_dev_reg_addr->d_data0 = FLASK_DMA_BUFFER_BASE_ADDR + devNo*PAGESIZE;
        setSTATUS(getSTATUS() & (~IECBITON));
            flash_dev_reg_addr->d_command = (saved_exception_state->s_a3 << BLOCKNUM_SHIFT) + READBLK_FLASH;
            int flash_status = SYSCALL(IOWAIT, &(mutex[flash_sem_idx]), 0, 0);
        setSTATUS(getSTATUS() | IECBITON);
        helper_copy_block(FLASK_DMA_BUFFER_BASE_ADDR + devNo*PAGESIZE, saved_exception_state->s_a1);
    SYSCALL(VERHO, &(mutex[flash_sem_idx]), 0, 0);

    if (flash_status == READY){
        saved_exception_state->s_v0 = flash_status;
    } else{
        saved_exception_state->s_v0 = 0 - flash_status;
    }
}

void WRITE_TO_FLASH(support_t *currentSupport){
    state_PTR saved_exception_state = &(currentSupport->sup_exceptState[GENERALEXCEPT]);
    int devNo = saved_exception_state->s_a2;
    int flash_sem_idx = devSemIdx(FLASHINT, devNo, FALSE);
    device_t *flash_dev_reg_addr = devAddrBase(FLASHINT, devNo);

    if ((saved_exception_state->s_a1 < KUSEG) || (saved_exception_state->s_a3 < 32) || (saved_exception_state->s_a3 >= flash_dev_reg_addr->d_data1)){
        program_trap_handler(currentSupport, NULL);
    }
    
    SYSCALL(PASSERN, &(mutex[flash_sem_idx]), 0, 0);
        helper_copy_block(saved_exception_state->s_a1, FLASK_DMA_BUFFER_BASE_ADDR + devNo*PAGESIZE);
        flash_dev_reg_addr->d_data0 = FLASK_DMA_BUFFER_BASE_ADDR + devNo*PAGESIZE;
        setSTATUS(getSTATUS() & (~IECBITON));
            flash_dev_reg_addr->d_command = (saved_exception_state->s_a3 << BLOCKNUM_SHIFT) + WRITEBLK_FLASH;
            int flash_status = SYSCALL(IOWAIT, &(mutex[flash_sem_idx]), 0, 0);
        setSTATUS(getSTATUS() | IECBITON);
    SYSCALL(VERHO, &(mutex[flash_sem_idx]), 0, 0);

    if (flash_status == READY){
        saved_exception_state->s_v0 = flash_status;
    } else{
        saved_exception_state->s_v0 = 0 - flash_status;
    }
}

void set_up_backing_store(){

}