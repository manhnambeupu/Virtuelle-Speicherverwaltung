/**
 * @file mmanage.c
 * @author Prof. Dr. Wolfgang Fohl, HAW Hamburg
 * @date  2014

 * @brief Memory Manager module of TI BSP A3 virtual memory
 * 
 * This is the memory manager process that
 * works together with the vmaccess process to
 * manage virtual memory management.
 *
 * mvappl sends synchronious command to memory manager via
 * module syncdataexchange.
 *
 * This process starts shared memory, so
 * it has to be started prior to the vmaccess process.
 *
 */

#include <signal.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "mmanage.h"
#include "debug.h"
#include "error.h"
#include "pagefile.h"
#include "logger.h"
#include "syncdataexchange.h"
#include "vmem.h"

/*
 * Signatures of private / static functions
 */

/**
 *****************************************************************************************
 *  @brief      This function fetchs a page from disk into memory. The page table 
 *              will be updated.
 *
 *  @param      page Number of the page that should be removed
 *  @param      frame Number of frame that should contain the page.
 * 
 *  @return     void 
 ****************************************************************************************/
static void fetch_page_from_disk(int page, int frame);//Lấy một trang từ đĩa vào bộ nhớ.

/**
 *****************************************************************************************
 *  @brief      This function removes a page from main memory. If the page was modified,
 *              it will be written back to disk. The page table will be updated.
 *
 *  @param      page Number of the page that should be removed
 * 
 *  @return     void 
 ****************************************************************************************/
static void remove_page_from_memory(int page);//Loại bỏ một trang khỏi bộ nhớ chính.

/**
 *****************************************************************************************
 *  @brief      This function initializes the virtual memory.
 *              In particular it creates the shared memory. The application just attachs 
 *              to the shared memory.
 *
 *  @return     void 
 ****************************************************************************************/
static void vmem_init(void);

/**
 *****************************************************************************************
 *  @brief      This function finds an unused frame. At the beginning all frames are 
 *              unused. A frame will never change it's state form used to unused.
 *
 *              Since the log files to be compared with contain the allocated frames, unused 
 *              frames must always be assigned the same way. Here, the frames are assigned 
 *              according to ascending frame number.
 *            
 *  @return     idx of the unused frame with the smallest idx. 
 *              If all frames are in use, VOID_IDX will be returned.
 ****************************************************************************************/
static int find_unused_frame();//Tìm khung không sử dụng trong bộ nhớ.

/**
 *****************************************************************************************
 *  @brief      This function will be called when a page fault has occurred. It allocates 
 *              a new page into memory. If all frames are in use the corresponding page 
 *              replacement algorithm will be called.
 *              Please take into account that allocate_page must update the page table 
 *              and log the page fault as well.
 *
 *  @param      req_page  The page that must be allocated due to the page fault. 

 *  @param      g_count   Current g_count value
 *
 *  @return     void 
 ****************************************************************************************/
static void allocate_page(const int req_page, const int g_count);//Phân bổ trang mới vào bộ nhớ trong trường hợp trang bị lỗi.

/**
 *****************************************************************************************
 *  @brief      This function is the signal handler attached to system call sigaction
 *              for signal SIGUSR2 and SIGINT.
 *              These signals have the same signal handler. Based on the parameter 
 *              signo the corresponding action will be started.
 *
 *  @param      signo Current signal that has be be handled.
 * 
 *  @return     void 
 ****************************************************************************************/
static void sighandler(int signo);

/**
 *****************************************************************************************
 *  @brief      This function dumps the page table to stderr.
 *
 *  @return     void 
 ****************************************************************************************/
static void dump_pt(void);//In ra thông tin bảng trang.

/**
 *****************************************************************************************
 *  @brief      This function implements page replacement algorithm aging.
 *
 *  @param      page Number of page that should be loaded into memory.
 *
 *  @param      removedPage Number of page that has been selected for replacement.
 *              If an unused frame has selected, this parameter will not be 
 *              modified.
 *
 *  @param      frame Number of frame that will be used to store the page.
 *
 ****************************************************************************************/
static void find_remove_aging(int page, int * removedPage, int *frame);//Thực hiện thuật toán thay thế trang theo tuổi.
 
/**
 *****************************************************************************************
 *  @brief      This function does aging for aging page replacement algorithm.
 *              It will be called periodic based on g_count.
 *              This function must be used only when aging algorithm is activ.
 *              Otherwise update_age_reset_ref may interfere with other page replacement 
 *              alogrithms that base on PTF_REF bit.
 *
 *  @return     void
 ****************************************************************************************/
static void update_age_reset_ref(void);//Cập nhật thông tin tuổi và đặt lại các bit tham chiếu cho thuật toán theo tuổi

/**
 *****************************************************************************************
 *  @brief      This function implements page replacement algorithm fifo.
 *
 *  @param      page Number of page that should be loaded into memory.
 *
 *  @param      removedPage Number of page that has been selected for replacement.
 *              If an unused frame has selected, this parameter will not be 
 *              modified.
 *
 *  @param      frame Number of frame that will be used to store the page.
 *
 ****************************************************************************************/
static void find_remove_fifo(int page, int * removedPage, int *frame);//Thực hiện thuật toán thay thế trang FIFO

/**
 *****************************************************************************************
 *  @brief      This function implements page replacement algorithm clock.
 *
 *  @param      page Number of page that should be loaded into memory.
 *
 *  @param      removedPage Number of page that has been selected for replacement.
 *              If an unused frame has selected, this parameter will not be 
 *              modified.
 *
 *  @param      frame Number of frame that will be used to store the page.
 *
 ****************************************************************************************/
static void find_remove_clock(int page, int * removedPage, int *frame);//Thực hiện thuật toán thay thế trang Clock

/**
 *****************************************************************************************
 *  @brief      This function cleans up when mmange runs out.
 *
 *  @return     void 
 ****************************************************************************************/
static void cleanup(void) ;//Dọn sạch tài nguyên khi mmanage kết thúc.

/**
 *****************************************************************************************
 *  @brief      This function scans all parameters of the porgram.
 *              The corresponding global variables page_rep_algo will be set.
 * 
 *  @param      argc number of parameter 
 *
 *  @param      argv parameter list 
 *
 *  @return     void 
 ****************************************************************************************/
static void scan_params(int argc, char **argv);//Quét các tham số dòng lệnh để xác định thuật toán thay thế trang.

/**
 *****************************************************************************************
 *  @brief      This function prints an error message and the usage information of 
 *              this program.
 *
 *  @param      err_str pointer to the error string that should be printed.
 *  @param      programName pointer to the name of the the program
 *
 *  @return     void 
 ****************************************************************************************/
static void print_usage_info_and_exit(char *err_str, char * programName); // In thông tin sử dụng và thoát nếu có tham số không đúng.

/*
 * variables for memory management
 */

static int pf_count = 0;               //!< page fault counter
static int shm_id = -1;                //!< shared memory id. Will be used to destroy shared memory when mmanage terminates

static void (*pageRepAlgo) (int, int*, int*) = NULL; //!< selected page replacement algorithm according to parameters of mmanage

/* information used for ageing replacement strategy. For each frame, which stores a valid page, 
 * the age and and the corresponding page will be stored.
 */

struct age {
   unsigned char age;  //!< 8 bit counter for aging page replacement algorithm
   int page;           //!< page belonging to this entry
 };

struct age age[VMEM_NFRAMES];

static struct vmem_struct *vmem = NULL; //!< Reference to shared memory

int main(int argc, char **argv) {
    struct sigaction sigact;
    init_pagefile(); // init page file
    open_logger();   // open logfile

    // Setup IPC for sending commands from vmapp to mmanager
    setupSyncDataExchange();

    // Create shared memory and init vmem structure 
    vmem_init();
    TEST_AND_EXIT_ERRNO(!vmem, "Error initialising vmem");
    PRINT_DEBUG((stderr, "vmem successfully created\n"));

    // init aging info
    for(int i = 0; i < VMEM_NFRAMES; i++) {
       age[i].page = VOID_IDX;
       age[i].age = 0;
    }

    // scan parameter 
    pageRepAlgo = find_remove_fifo;
    scan_params(argc, argv);

    /* Setup signal handler */
    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART; // damit mq_receive man eine signal neu gestartet wird

    TEST_AND_EXIT_ERRNO(sigaction(SIGUSR2, &sigact, NULL) == -1, "Error installing signal handler for USR2");
    PRINT_DEBUG((stderr,  "USR2 handler successfully installed\n"));

    TEST_AND_EXIT_ERRNO(sigaction(SIGINT, &sigact, NULL) == -1, "Error installing signal handler for INT");
    PRINT_DEBUG((stderr, "INT handler successfully installed\n"));

    // Server Loop, waiting for commands from vmapp
    while(1) {
		struct msg m = waitForMsg();
        switch(m.cmd){
			case CMD_PAGEFAULT:
				allocate_page(m.value, m.g_count);
				break;
			case CMD_TIME_INTER_VAL:
                if (pageRepAlgo == find_remove_aging) {
                   update_age_reset_ref();
                }
				break;
			default:
				TEST_AND_EXIT(true, (stderr, "Unexpected command received from vmapp\n"));
        }
        sendAck();
    }
    return 0;
}

void scan_params(int argc, char **argv) {
    int i = 0;
    bool param_ok = false;
    char * programName = argv[0];

    // scan all parameters (argv[0] points to program name)
    if (argc > 2) print_usage_info_and_exit("Wrong number of parameters.\n", programName);

    for (i = 1; i < argc; i++) {
        param_ok = false;
        if (0 == strcasecmp("-fifo", argv[i])) {
            // page replacement strategies fifo selected 
            pageRepAlgo = find_remove_fifo;
            param_ok = true;
        }
        if (0 == strcasecmp("-clock", argv[i])) {
            // page replacement strategies clock selected 
            pageRepAlgo = find_remove_clock;
            param_ok = true;
        }
        if (0 == strcasecmp("-aging", argv[i])) {
            // page replacement strategies aging selected 
            pageRepAlgo = find_remove_aging;
            param_ok = true;
        }
        if (!param_ok) print_usage_info_and_exit("Undefined parameter.\n", programName); // undefined parameter found
    } // for loop
}

void print_usage_info_and_exit(char *err_str, char *programName) {
	fprintf(stderr, "Wrong parameter: %s\n", err_str);
	fprintf(stderr, "Usage : %s [OPTIONS]\n", programName);
	fprintf(stderr, " -fifo     : Fifo page replacement algorithm.\n");
	fprintf(stderr, " -clock    : Clock page replacement algorithm.\n");
	fprintf(stderr, " -aging    : Aging page replacement algorithm.\n");
	fprintf(stderr, " -pagesize=[8,16,32,64] : Page size.\n");
	fflush(stderr);
	exit(EXIT_FAILURE);
}

/*
    SIGUSR2(User-defined Signal2) : tín hiệu này để thông báo giữa các quá trình
    SIGINT(Interrupt Signal): tín hiệu gửi khi Ctr C
*/
void sighandler(int signo) {
if(signo == SIGUSR2) {
        dump_pt();
    } else if(signo == SIGINT) {
        cleanup();
        exit(EXIT_SUCCESS);
    }  
}

void dump_pt(void) {
    int i;
    int ncols = 8;

    fprintf(stderr,
            "\n======================================\n"
            "\tPage Table Dump\n");

    fprintf(stderr, "VIRT MEM SIZE    = \t %d\n", VMEM_VIRTMEMSIZE);
    fprintf(stderr, "PHYS MEM SIZE    = \t %d\n", VMEM_PHYSMEMSIZE);
    fprintf(stderr, "PAGESIZE         = \t %d\n", VMEM_PAGESIZE);
    fprintf(stderr, "Number of Pages  = \t %d\n", VMEM_NPAGES);
    fprintf(stderr, "Number of Frames = \t %d\n", VMEM_NFRAMES);

    fprintf(stderr, "======================================\n");
    fprintf(stderr, "shm_id: \t %x\n", shm_id);
    fprintf(stderr, "pf_count: \t %d\n", pf_count);
    for(i = 0; i < VMEM_NPAGES; i++) {
        fprintf(stderr,
			"Page %5d, Flags %x, Frame %10d, age 0x%2X,  \n", i,
            vmem->pt[i].flags, vmem->pt[i].frame, age[vmem->pt[i].frame].age);
    }
    fprintf(stderr,
            "\n\n======================================\n"
            "\tData Dump\n");
    for(i = 0; i < (VMEM_NFRAMES * VMEM_PAGESIZE); i++) {
        fprintf(stderr, "%10x", vmem->mainMemory[i]);
        if(i % ncols == (ncols - 1)) {
            fprintf(stderr, "\n");
        }
        else {
            fprintf(stderr, "\t");
        }
    }
}

/* Your code goes here... */

void cleanup(void) {
    // Đóng tệp nhật ký
    close_logger();

    // Giải phóng tài nguyên của pagefile và shared memory
    cleanup_pagefile();

    if (shm_id != -1) {
        // Nếu đã attach shared memory, thì detach nó
        if (vmem != NULL) {
            shmdt(vmem);
            vmem = NULL;
        }

        // Nếu không còn process nào attach shared memory, thì xóa nó
        struct shmid_ds shmid_ds;
        if (shmctl(shm_id, IPC_STAT, &shmid_ds) != -1) {
            if (shmid_ds.shm_nattch == 0) {
                shmctl(shm_id, IPC_RMID, NULL);
            }
        }
    }
}


void vmem_init(void) {
    // Generate a key for shared memory
    key_t key = ftok(SHMKEY, SHMPROCID);
    TEST_AND_EXIT_ERRNO(key == -1, "Error creating key with ftok");

    // Get shared memory segment
    shm_id = shmget(key, SHMSIZE, 0664 | IPC_CREAT);
    TEST_AND_EXIT_ERRNO(shm_id == -1, "Error creating shared memory with shmget");

    // Attach shared memory to vmem
    vmem = (struct vmem_struct *)shmat(shm_id, NULL, 0);
    TEST_AND_EXIT_ERRNO(vmem == (struct vmem_struct *)-1, "Error attaching shared memory with shmat");

    // Fill with zeros
    memset(vmem, 0, SHMSIZE);
}

int find_unused_frame() {
    for (int i = 0; i < VMEM_NFRAMES; i++) {
        if (vmem->pt[i].flags == PTF_UNUSED) {
            // Frame không được sử dụng, trả về chỉ số của nó
            return i;
        }
    }

    // Nếu không có frame trống nào được tìm thấy, trả về VOID_IDX hoặc một giá trị thích hợp
    return VOID_IDX;
}

void allocate_page(const int req_page, const int g_count) {
    int frame = VOID_IDX;
    int removedPage = VOID_IDX;
    struct logevent le; 

    /* Log action */
    le.req_pageno = req_page;
    le.replaced_page = removedPage;
    le.alloc_frame = frame;
    le.g_count = g_count; 
    le.pf_count = pf_count;
    logger(le); 
}

void fetch_page_from_disk(int page, int frame){
        // Kiểm tra tính hợp lệ của page và frame
    if (page < 0 || page >= VMEM_NPAGES) {
        printf("Error: Invalid page number!");
        return;
    }

    if (frame < 0 || frame >= VMEM_NFRAMES){ 
        printf("Error: Invalid frame number!");
        return;
    }

    // Đọc trang từ file pagefile vào khung bộ nhớ
    fetch_page_from_pagefile(page, vmem->mainMemory + frame*VMEM_PAGESIZE);
    
    // Cập nhật bảng trang
    vmem->pt[page].frame = frame;
    vmem->pt[page].flags |= PTF_PRESENT; // Đánh dấu trang đã có trong bộ nhớ

    printf("Page loaded %d to the frame %d\n", page, frame);
}

void remove_page_from_memory(int page){
  // Kiểm tra tính hợp lệ của số trang
  if (page < 0 || page >= VMEM_NPAGES) {
    printf("Error: Invalid page number!");
    return;
  }
  // Lấy số khung chứa trang cần loại bỏ
  int frame = vmem->pt[page].frame;
  // Nếu trang đã bị thay đổi, ghi nó vào pagefile trước
  if (vmem->pt[page].flags & PTF_DIRTY) {
    store_page_to_pagefile(page, vmem->mainMemory + frame * VMEM_PAGESIZE);
  }

  // Giải phóng khung đó trong bộ nhớ chính
  vmem->pt[page].frame = VOID_IDX;  
  vmem->pt[page].flags = PTF_UNUSED; 

  printf("Page removed %d from the frame %d\n", page, frame);
}

void find_remove_fifo(int page, int * removedPage, int *frame){
     //Duyệt tất cả các khung để tìm khung chứa trang cũ nhất  
    for (int i = 0; i < VMEM_NFRAMES; i++) { 
        if (vmem->pt[i].flags & PTF_PRESENT) { //Nếu khung chứa trang hợp lệ 
            //Kiểm tra xem trang này có phải là trang cũ nhất không 
            if (vmem->pt[i].frame == *removedPage) { 
                //Nếu đúng, gán khung này cho trang mới và thoát khỏi vòng lặp 
                *frame = i; 
                vmem->pt[page].frame = i; 
                vmem->pt[page].flags |= PTF_PRESENT; 
                return; 
            } 
            //Nếu không phải, cập nhật removedPage bằng trang cũ hơn 
            else if (vmem->pt[i].frame < *removedPage) { 
                *removedPage = vmem->pt[i].frame; 
            } 
        } 
    } 
    //Nếu không tìm thấy khung trống nào, trả về VOID_IDX  
    *frame = VOID_IDX; 
}

static void find_remove_clock(int page, int * removedPage, int *frame){
    // Duyệt tất cả các khung để tìm khung chứa trang cần thay thế
    for (int i = 0; i < VMEM_NFRAMES; i++) {
        if (vmem->pt[i].flags & PTF_PRESENT) { //Nếu khung chứa trang hợp lệ
            //Kiểm tra xem trang này có phải là ứng cử viên thay thế không
            if (vmem->pt[i].frame == *removedPage) {
                //Nếu đúng, gán khung này cho trang mới và thoát khỏi vòng lặp
                *frame = i;
                vmem->pt[page].frame = i;
                vmem->pt[page].flags |= PTF_PRESENT;
                return;
            } else {
                //Nếu không phải, kiểm tra xem trang này có được truy cập gần đây không
                if (!(vmem->pt[i].flags & PTF_REF)) {
                    //Nếu không, cập nhật removedPage bằng khung này
                    *removedPage = vmem->pt[i].frame;
                } else {
                    //Nếu có, đặt lại bit tham chiếu và tiếp tục duyệt
                    vmem->pt[i].flags &= ~PTF_REF;
                }
            }
        }
    }
}

static void find_remove_aging(int page, int * removedPage, int *frame){
}

static void update_age_reset_ref(void){
    // Duyệt tất cả các khung có chứa trang hợp lệ
    for (int i = 0; i < VMEM_NFRAMES; i++) {
        if (vmem->pt[age[i].page].flags & PTF_PRESENT) {
            // Tăng tuổi của trang
            age[i].age++; 

            // Nếu tuổi của trang vượt quá giới hạn, đặt lại nó về 0
            // 8 bit = 0x7F
            if (age[i].age > 0x7F) {
                age[i].age = 0;
            }

            // Đặt lại bit tham chiếu của trang
            vmem->pt[age[i].page].flags &= ~PTF_REF;
        }
} 

// EOF
