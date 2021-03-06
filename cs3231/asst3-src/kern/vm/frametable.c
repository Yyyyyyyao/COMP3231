#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <spl.h>

#include <machine/vm.h>

/* Place your frametable data-structures here 
 * You probably also want to write a frametable initialisation
 * function and call it from vm_bootstrap
 */
/* layout of a frame table entry */
struct frame_entry {
    //int       fe_refcount;        /* number of references to this frame */
    int     fe_used;            /* flag to indicate if this frame is free */
    int     fe_next;            /* if this frame is free, index of next free */
};

/* pointer to the frame table */
struct frame_entry *ft;                 


int cur_free;


//static vaddr_t pop_frame(void);
//static void push_frame(vaddr_t vaddr);

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

/* Note that this function returns a VIRTUAL address, not a physical 
 * address
 * WARNING: this function gets called very early, before
 * vm_bootstrap().  You may wish to modify main.c to call your
 * frame table initialisation function, or check to see if the
 * frame table has been initialised and call ram_stealmem() otherwise.
 */

void frametable_init(void){
    paddr_t ram_sz, ft_top;
        size_t ft_size; 
        int n_pages, used_pages, i;

        /* size of ram and num pages in the system */
        ram_sz = ROUND_UP(ram_getsize());
        n_pages = ram_sz / PAGE_SIZE;

        /* set the size to init the hpt when we need to */
        //hpt_size = n_pages * 2;

        kprintf("[*] Virtual Memory: %d frame fit in memory\n", n_pages);
        kprintf("[*] Virtual Memory: ram size is %p\n", (void *)ram_sz);

        /* calc size of the frame table */
        ft_size = n_pages * sizeof(struct frame_entry);
        kprintf("[*] Virtual Memory: size of ft is: 0x%x\n", (int)ft_size);

        /* allocate some space for the hpt when we need it */
        //hpt = (struct page_entry **)kmalloc(hpt_size * sizeof(struct page_entry *));

        /* allocate the frame table above the os */
        ft = (struct frame_entry *)kmalloc(ft_size);

        /* calc the top of the frame table */
        ft_top = ROUND_UP(ram_getfirstfree());
        kprintf("[*] Virtual Memory: first free is %p\n", (void *)ft_top);

        /* calculate the number of pages used so far */
        used_pages = ft_top / PAGE_SIZE;
        kprintf("[*] Virtual Memory: num pages used in total: %d\n", used_pages);
        kprintf("[*] Virtual Memory: therefore size of used: 0x%x\n", used_pages*PAGE_SIZE);

        /* set the current free page index */
        cur_free = used_pages;

        /* then init all the dirty pages */
        for(i = 0; i < used_pages; i++)
        {
                //ft[i].fe_refcount = 1;
                ft[i].fe_used = 1;
                ft[i].fe_next = VM_INVALID_INDEX;
        }
        /* init the clean pages */
        for(i = used_pages; i < n_pages; i++)
        {
                //ft[i].fe_refcount = 0;
                ft[i].fe_used = 0;
                if(i != n_pages-1) {
                        ft[i].fe_next = i+1;
                }
        }
        /* fix the corner case */
        ft[n_pages-1].fe_next = VM_INVALID_INDEX;
}


vaddr_t alloc_kpages(unsigned int npages)
{
        /*
         * IMPLEMENT ME.  You should replace this code with a proper
         *                implementation
         */

    if (!ft) {
                /* vm system not alive - use stealmem */
                paddr_t addr;
                spinlock_acquire(&stealmem_lock);
                addr = ram_stealmem(npages);
                spinlock_release(&stealmem_lock);
                if(addr == 0) {
                        return 0;
                }
                return PADDR_TO_KVADDR(addr);
        } else {
                /* use my allocator as frame table is now initialised */
                if (npages > 1){
                        /* can't alloc more than one page */
                        return 0;
                }
                spinlock_acquire(&stealmem_lock);
                /* ensure we have enough memory to alloc */
                if (cur_free == VM_INVALID_INDEX) {
                        spinlock_release(&stealmem_lock);
                        return 0;
                }
                /* pop the next free frame */         
                //vaddr_t addr = pop_frame();


                int c_index = cur_free;
        /* if we are getting the last frame, deem the cur_free invalid */
		        if (ft[cur_free].fe_next == VM_INVALID_INDEX) {
		                cur_free = VM_INVALID_INDEX;
		        } else {
		                /* otherwise just set the next free */
		                cur_free = ft[cur_free].fe_next;
		        }


		        /* alter meta data */
		        ft[c_index].fe_used = 1;
		        //ft[c_index].fe_refcount = 1;
		        ft[c_index].fe_next = VM_INVALID_INDEX;

		        //vaddr_t addr = FINDEX_TO_KVADDR(c_index);       /* find the kvaddr */
		        vaddr_t addr = PADDR_TO_KVADDR(c_index << 12);

		        bzero((void *)addr, PAGE_SIZE);                 /* zero the frame */




                spinlock_release(&stealmem_lock);
                return addr;               
        }
        
}

// static vaddr_t pop_frame(void)
// {
//         int c_index = cur_free;
//         /* if we are getting the last frame, deem the cur_free invalid */
//         if (ft[cur_free].fe_next == VM_INVALID_INDEX) {
//                 cur_free = VM_INVALID_INDEX;
//         } else {
//                 /* otherwise just set the next free */
//                 cur_free = ft[cur_free].fe_next;
//         }


//          alter meta data 
//         ft[c_index].fe_used = 1;
//         ft[c_index].fe_refcount = 1;
//         ft[c_index].fe_next = VM_INVALID_INDEX;

//         //vaddr_t addr = FINDEX_TO_KVADDR(c_index);       /* find the kvaddr */
//         vaddr_t addr = PADDR_TO_KVADDR(c_index << 12);

//         bzero((void *)addr, PAGE_SIZE);                 /* zero the frame */
        
//         return addr;
// }


// static void push_frame(vaddr_t vaddr)
// {
//         int c_index;
//         //c_index = KVADDR_TO_FINDEX(vaddr);

//         c_index = (KVADDR_TO_PADDR(vaddr) >> 12);

//         /* append fe to the start of the freelist */
//         if (ft[c_index].fe_refcount == 1) {
//                 ft[c_index].fe_used = 0;
//                 ft[c_index].fe_refcount = 0;
//                 ft[c_index].fe_next = cur_free;
//                 cur_free = c_index;
//         } else if (ft[c_index].fe_refcount == 0) {
//                 panic("reached 0 refcount - this should never happen\n");
//         } else {
//                 ft[c_index].fe_refcount--;
//  //               kprintf("vaddr %x has new refcount %d\n", vaddr, ft[c_index].fe_refcount); 
//         }
// }

void free_kpages(vaddr_t addr)
{
        spinlock_acquire(&stealmem_lock);
        /* call static function to free */
        // push_frame(addr);
        int c_index;
        //c_index = KVADDR_TO_FINDEX(vaddr);

        c_index = (KVADDR_TO_PADDR(addr) >> 12);

                ft[c_index].fe_used = 0;
                //ft[c_index].fe_refcount = 0;
                ft[c_index].fe_next = cur_free;
                cur_free = c_index;

        /* append fe to the start of the freelist */
 //        if (ft[c_index].fe_refcount == 1) {
 //                ft[c_index].fe_used = 0;
 //                ft[c_index].fe_refcount = 0;
 //                ft[c_index].fe_next = cur_free;
 //                cur_free = c_index;
 //        } else if (ft[c_index].fe_refcount == 0) {
 //                panic("reached 0 refcount - this should never happen\n");
 //        } else {
 //                ft[c_index].fe_refcount--;
 // //               kprintf("vaddr %x has new refcount %d\n", vaddr, ft[c_index].fe_refcount); 
 //        }
        spinlock_release(&stealmem_lock);
}


