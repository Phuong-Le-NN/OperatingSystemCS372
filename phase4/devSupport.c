#include "devSupport.h"
#include "../h/const.h"

HIDDEN void helper_copy_block(int *src, int *dst){
    int i;
    for (i = 0; i < (BLOCKSIZE/4); i++){
        *dst = *src;
        dst++; /*should increase by 4*/
        src++;
    }
}

void WRITE_TO_DISK(support_t *currentSupport){
    state_PTR saved_gen_exc_state = &(currentSupport->sup_exceptState[GENERALEXCEPT]);

    int devNo = saved_gen_exc_state->s_a2;
    int disk_sem_idx = devSemIdx(DISKINT, devNo, FALSE);

    device_t *disk_dev_reg_addr = devAddrBase(DISKINT, devNo);

    int maxcyl = ((disk_dev_reg_addr->d_data1) >> 16) & 0xFFFF;
    int maxhead = ((disk_dev_reg_addr->d_data1) >> 8) & 0xFF;
    int maxsect = (disk_dev_reg_addr->d_data1) & 0xFF;

    if ((saved_gen_exc_state->s_a1 < KUSEG) || (saved_gen_exc_state->s_a3 > (maxcyl*maxhead*maxsect))){         /*when setting up backing store as disk, depending on where on disk storing an image, it should be illegal to write there too*/
        program_trap_handler(currentSupport, NULL);
    }

    SYSCALL(PASSERN, &(mutex[disk_sem_idx]), 0, 0);
        int sectNo = (saved_gen_exc_state->s_a3) % maxsect;
	    int headNo = ((int) ((saved_gen_exc_state->s_a3) / (maxsect * maxcyl))) % maxhead; /*divide and round down*/
        int cylNo = ((int) ((saved_gen_exc_state->s_a3) / maxsect)) % maxcyl;
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (cylNo << CYLNUM_SHIFT) + SEEKCYL; /*seek*/
            int disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
        if (disk_status != READY){
            saved_gen_exc_state->s_v0 = 0 - disk_status;
            SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);
            return;
        }
        helper_copy_block(saved_gen_exc_state->s_a1, DISK_DMA_BUFFER_BASE_ADDR + (BLOCKSIZE*devNo));
        disk_dev_reg_addr->d_data0 = DISK_DMA_BUFFER_BASE_ADDR + (BLOCKSIZE*devNo); /*carefull not to do anything with device between writing to data0 and command*/
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (headNo << HEADNUM_SHIFT) + (sectNo << SECTNUM_SHIFT) + WRITEBLK_DSK; /*write*/
            disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
    SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);

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

    int maxcyl = ((disk_dev_reg_addr->d_data1) >> 16) & 0xFFFF;
    int maxhead = ((disk_dev_reg_addr->d_data1) >> 8) & 0xFF;
    int maxsect = (disk_dev_reg_addr->d_data1) & 0xFF;

    if ((saved_gen_exc_state->s_a1 < KUSEG) || (saved_gen_exc_state->s_a3 > (maxcyl*maxhead*maxsect))){         /*when setting up backing store as disk, depending on where on disk storing an image, it should be illegal to write there too*/
        program_trap_handler(currentSupport, NULL);
    }

    SYSCALL(PASSERN, &(mutex[disk_sem_idx]), 0, 0);
        int sectNo = saved_gen_exc_state->s_a3 % maxsect;
        int headNo = ((int) (saved_gen_exc_state->s_a3 / (maxsect * maxcyl))) % maxhead;
        int cylNo = ((int) (saved_gen_exc_state->s_a3 / maxsect)) % maxcyl;
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (cylNo << CYLNUM_SHIFT) + SEEKCYL;
            int disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
        if (disk_status != READY){
            saved_gen_exc_state->s_v0 = 0 - disk_status;
            SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);
            return;
        }
        disk_dev_reg_addr->d_data0 = DISK_DMA_BUFFER_BASE_ADDR + (BLOCKSIZE*devNo);
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (headNo << HEADNUM_SHIFT) + (sectNo << SECTNUM_SHIFT) + READBLK_DSK;
            disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
        helper_copy_block(DISK_DMA_BUFFER_BASE_ADDR + (BLOCKSIZE*devNo), saved_gen_exc_state->s_a1);
    SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);

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

    if ((saved_exception_state->s_a1 < KUSEG) || (saved_exception_state->s_a3 < 32) || (saved_exception_state->s_a3 >= flash_dev_reg_addr->d_data1)){ /* should it be illegal to read from backing store area in flash?*/
        program_trap_handler(currentSupport, NULL);
    }

    SYSCALL(PASSERN, &(mutex[flash_sem_idx]), 0, 0);
        flash_dev_reg_addr->d_data0 = FLASK_DMA_BUFFER_BASE_ADDR + BLOCKSIZE*devNo;
        setSTATUS(getSTATUS() & (~IECBITON));
            flash_dev_reg_addr->d_command = (saved_exception_state->s_a3 << BLOCKNUM_SHIFT) + READBLK_FLASH;
            int flash_status = SYSCALL(IOWAIT,FLASHINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
        helper_copy_block(FLASK_DMA_BUFFER_BASE_ADDR + BLOCKSIZE*devNo, saved_exception_state->s_a1);
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
        helper_copy_block(saved_exception_state->s_a1, FLASK_DMA_BUFFER_BASE_ADDR + BLOCKSIZE*devNo);
        flash_dev_reg_addr->d_data0 = FLASK_DMA_BUFFER_BASE_ADDR + BLOCKSIZE*devNo;
        setSTATUS(getSTATUS() & (~IECBITON));
            flash_dev_reg_addr->d_command = (saved_exception_state->s_a3 << BLOCKNUM_SHIFT) + WRITEBLK_FLASH;
            int flash_status = SYSCALL(IOWAIT, FLASHINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
    SYSCALL(VERHO, &(mutex[flash_sem_idx]), 0, 0);

    if (flash_status == READY){
        saved_exception_state->s_v0 = flash_status;
    } else{
        saved_exception_state->s_v0 = 0 - flash_status;
    }
}