/**
 * @file vmem.h
 * @author Wolfgang Fohl, HAW Hamburg
 * @brief Global Definitions for TI BS A3 - model of virtual memory management

 * First Version : Wolfgang Fohl HAW Hamburg
 * Dec 2015 : Delete BitMap for free frames (Franz Korf, HAW Hamburg)
 * Dec 2015 : Set memory algorithm vi command line parameter
 * Dec 2015 : Set define for PAGESIZE and VMEM_ALGO via compiler -D option (Franz Korf, HAW Hamburg)
 * Dec 2015 : Add some documentation (Franz Korf, HAW Hamburg)
 * April 2018 : New IPC for mmanage and vmappl (Franz Korf, HAW Hamburg)
 * May   2022 : Change to byte machine
 */

#ifndef VMEM_H
#define VMEM_H

#define SHMKEY          "./src/vmem.h" //!< First paremater for shared memory generation via ftok function
#define SHMPROCID       1234           //!< Second paremater for shared memory generation via ftok function

/**
 * Constant VMEM_PAGESIZE will be sete via compiler -D option.
 * Default value : 8
 * value range : 8 16 32 64
 */
#ifndef VMEM_PAGESIZE
#define VMEM_PAGESIZE 8
#endif

/* Sizes */
#define VMEM_VIRTMEMSIZE 1024  //Kích thước của không gian địa chỉ ảo của quá trình. 				//!< Size of virtual address space of the process
#define VMEM_PHYSMEMSIZE  128   				//!< Size of physical memory
#define VMEM_NPAGES     (VMEM_VIRTMEMSIZE / VMEM_PAGESIZE)	//!< Total number of pages
#define VMEM_NFRAMES (VMEM_PHYSMEMSIZE / VMEM_PAGESIZE)		//!< Total number of (page) frames

/**
 * page table flags used by this simulation
 */
#define PTF_PRESENT     1 //Trang hiện đang được chọn (present).
#define PTF_DIRTY       2 // Trang có thay đổi và cần được ghi
#define PTF_REF         4 // Tham chiếu đến trang
#define PTF_UNUSED		0 
#define VOID_IDX 		-1       //!< Constant for invalid page or frame reference

/**
 * Page table entry
 */
 //Định nghĩa một mục trong bảng trang với các trạng thái và thông tin về khung.
struct pt_entry {
	int flags;             //!< See definition of PTF_* flags
	int frame;             //!< Frame idx; frame == VOID_IDX: unvalid reference
};

/**
 * The data structure stored in shared memory
 */
struct vmem_struct {
	struct pt_entry pt[VMEM_NPAGES];//bảng trang             //!< page table
	unsigned char mainMemory[VMEM_NFRAMES * VMEM_PAGESIZE];  //!< main memory used by virtual memory simulation
};

#define SHMSIZE (sizeof(struct vmem_struct)) //!< size of virtual memory

#endif /* VMEM_H */
