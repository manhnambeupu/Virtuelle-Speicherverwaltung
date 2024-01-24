/**
 * @file vmaccess.c
 * @author Prof. Dr. Wolfgang Fohl, HAW Hamburg
 * @date 2010
 * @brief The access functions to virtual memory.
 */

#include "vmaccess.h"
#include <sys/ipc.h>
#include <sys/shm.h>
//#include "syncdataexchange.c"
#include "syncdataexchange.h"
#include "vmem.h"
#include "debug.h"
#include "error.h"

/*
 * static variables
 */

static struct vmem_struct *vmem = NULL; //!< Reference to virtual memory

/**
 * The progression of time is simulated by the counter g_count, which is incremented by
 * vmaccess on each memory access. The memory manager will be informed by a command, whenever
 * a fixed period of time has passed. Hence the memory manager must be informed, whenever
 * g_count % TIME_WINDOW == 0.
 * Based on this information, memory manager will update aging information
 */

static int g_count = 0;    //!< global acces counter as quasi-timestamp - will be increment by each memory access
#define TIME_WINDOW   20

/**
 *****************************************************************************************
 *  @brief      This function setup the connection to virtual memory.
 *              The virtual memory has to be created by mmanage.c module.
 *
 *  @return     void
 ****************************************************************************************/
static void vmem_init(void) {
    /* Create System V shared memory */
    /* Get shared memory id from mmanage */
    //Tạo hoặc truy cập một đoạn bộ nhớ chia sẻ.
    key_t shm_key = ftok(SHMKEY_SYNC_COM, SHMPROCID_SYNC_COM);
    TEST_AND_EXIT_ERRNO(shm_key == -1, "vmem_init: ftok failed!");
    /* We are only using the shm, don't set the IPC_CREAT flag */
    //Gắn một đoạn bộ nhớ chia sẻ đã tồn tại vào không gian địa chỉ của một quy trình.
    //Zugang 0664 : bộ nhớ chia sẻ có quyền đọc và ghi 
    int shm_id = shmget(shm_key, SHMSIZE, 0664);
    TEST_AND_EXIT_ERRNO(shm_id == -1, "vmem_init: shmget failed!");
    /* Attach shared memory to vmem */
    //Gắn shared mem vào vmem
    vmem = (struct vmem_struct *)shmat(shm_id, NULL, 0);
    TEST_AND_EXIT_ERRNO(vmem == (struct vmem_struct *)-1, "Error attaching shared memory with shmat");
}

/**
 *****************************************************************************************
 *  @brief      This function puts a page into memory (if required). Ref Bit of page table
 *              entry will be updated.
 *              If the time window handle by g_count has reached, the window window message
 *              will be send to the memory manager.
 *              To keep conform with this log files, g_count must be increased before
 *              the time window will be checked.
 *              vmem_read and vmem_write call this function.
 *
 *  @param      address The page that stores the contents of this address will be
 *              put in (if required).
 *
 *  @return     void
 ****************************************************************************************/
static void vmem_put_page_into_mem(int address) {
    //Speicherzugriffe durch vmaccess gezählt wird.
    g_count++;

    /* Check if the time window is reached */
    if (g_count % TIME_WINDOW == 0) {
        struct msg timeIntervalMsg;
        timeIntervalMsg.cmd = CMD_TIME_INTER_VAL;
        timeIntervalMsg.value = 0;
        timeIntervalMsg.g_count = g_count;

        /* Send message to memory manager to handle time interval */
        sendMsgToMmanager(timeIntervalMsg);
    }

    /* Get the page number from the address */
    int page = address / VMEM_PAGESIZE;

    /* Check if the page is not in memory (page fault) */
    if (!(vmem->pt[page].flags & PTF_PRESENT)) {
        struct msg pageFaultMsg;
        pageFaultMsg.cmd = CMD_PAGEFAULT;
        pageFaultMsg.value = page;
        pageFaultMsg.g_count = g_count;

        /* Send message to memory manager to handle page fault */
        sendMsgToMmanager(pageFaultMsg);
    }

    if (vmem == NULL){
        vmem_init();
    }
    /* Update the reference bit of the page table entry */
    vmem->pt[page].flags |= PTF_REF;
}

unsigned char vmem_read(int address) {
     /* Put the required page into memory */
    vmem_put_page_into_mem(address);

    /* Get the value from memory */
    return vmem->mainMemory[address];
}

void vmem_write(int address, unsigned char data) {
     /* Put the required page into memory */
    vmem_put_page_into_mem(address);

    /* Update the value in memory */
    vmem->mainMemory[address] = data;
}
// EOF
