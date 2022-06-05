/**********************************************************************
 * Copyright (c) 2020-2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "types.h"
#include "list_head.h"
#include "vm.h"

/**
 * Ready queue of the system
 */
extern struct list_head processes;

/**
 * Currently running process
 */
extern struct process *current;

/**
 * Page Table Base Register that MMU will walk through for address translation
 */
extern struct pagetable *ptbr;

/**
 * TLB of the system.
 */
extern struct tlb_entry tlb[1UL << (PTES_PER_PAGE_SHIFT * 2)];


/**
 * The number of mappings for each page frame. Can be used to determine how
 * many processes are using the page frames.
 */
extern unsigned int mapcounts[];

void dump_status(void)
{
        struct process *p;

        list_for_each_entry_reverse(p, &processes, list){
                fprintf(stderr, "pid:%u\n", p->pid);
	}
}

/**
 * lookup_tlb(@vpn, @pfn)
 *
 * DESCRIPTION
 *   Translate @vpn of the current process through TLB. DO NOT make your own
 *   data structure for TLB, but use the defined @tlb data structure
 *   to translate. If the requested VPN exists in the TLB, return true
 *   with @pfn is set to its PFN. Otherwise, return false.
 *   The framework calls this function when needed, so do not call
 *   this function manually.
 *
 * RETURN
 *   Return true if the translation is cached in the TLB.
 *   Return false otherwise
 */
bool lookup_tlb(unsigned int vpn, unsigned int *pfn)
{
	for(int i=0;i<NR_TLB_ENTRIES;i++){
		//hit
		if(tlb[i].valid && tlb[i].vpn == vpn){

			int pd_idx = vpn/NR_PTES_PER_PAGE;
			int pte_idx = vpn%NR_PTES_PER_PAGE;
			
			*pfn = current->pagetable.outer_ptes[pd_idx]->ptes[pte_idx].pfn;
			tlb[i].pfn = *pfn;

			return true;
		}
	}
	
	//miss
	return false;
}


/**
 * insert_tlb(@vpn, @pfn)
 *
 * DESCRIPTION
 *   Insert the mapping from @vpn to @pfn into the TLB. The framework will call
 *   this function when required, so no need to call this function manually.
 *
 */
void insert_tlb(unsigned int vpn, unsigned int pfn)
{
	int full = 1;

	for(int i=0;i<NR_TLB_ENTRIES;i++){
		if(!tlb[i].valid){
			full = 0;
			tlb[i].valid = 1;
			tlb[i].vpn = vpn;
			tlb[i].pfn = pfn;
			break;
		}
	}
	//TLB full이면 FIFO
	if(full){
		for(int i=0;i<NR_TLB_ENTRIES-1;i++){
			tlb[i].vpn = tlb[i+1].vpn;
			tlb[i].pfn = tlb[i+1].pfn;
		}
		tlb[NR_TLB_ENTRIES-1].valid = 0;
		tlb[NR_TLB_ENTRIES-1].vpn = 0;
		tlb[NR_TLB_ENTRIES-1].pfn;
	}
}


/**
 * alloc_page(@vpn, @rw)
 *
 * DESCRIPTION
 *   Allocate a page frame that is not allocated to any process, and map it
 *   to @vpn. When the system has multiple free pages, this function should
 *   allocate the page frame with the **smallest pfn**.
 *   You may construct the page table of the @current process. When the page
 *   is allocated with RW_WRITE flag, the page may be later accessed for writes.
 *   However, the pages populated with RW_READ only should not be accessed with
 *   RW_WRITE accesses.
 *
 * RETURN
 *   Return allocated page frame number.
 *   Return -1 if all page frames are allocated.
 */
unsigned int alloc_page(unsigned int vpn, unsigned int rw)
{
	
	int pd_idx = vpn/NR_PTES_PER_PAGE;	//first
	int pte_idx = vpn%NR_PTES_PER_PAGE;	//second
	
	
	//null일때
	if(!current->pagetable.outer_ptes[pd_idx]){
		current->pagetable.outer_ptes[pd_idx] = (struct pte_directory*)malloc(sizeof(struct pte_directory));
	}
	
	//valid
	current->pagetable.outer_ptes[pd_idx]->ptes[pte_idx].valid = 1;
	
	//writable
	if(rw == RW_WRITE + 1){
		current->pagetable.outer_ptes[pd_idx]->ptes[pte_idx].writable = 1;
	}
	else{
		current->pagetable.outer_ptes[pd_idx]->ptes[pte_idx].writable = 0;
	}
	//pfn
	for(int i=0;i<NR_PTES_PER_PAGE;i++)		
	{
		if(mapcounts[i] == 0){
			mapcounts[i]++;
			current->pagetable.outer_ptes[pd_idx]->ptes[pte_idx].pfn = i;
			
			return i;
		}
	}
	
	return -1;

}


/**
 * free_page(@vpn)
 *
 * DESCRIPTION
 *   Deallocate the page from the current processor. Make sure that the fields
 *   for the corresponding PTE (valid, writable, pfn) is set @false or 0.
 *   Also, consider carefully for the case when a page is shared by two processes,
 *   and one process is to free the page.
 */
void free_page(unsigned int vpn)
{
	int pd_idx = vpn/NR_PTES_PER_PAGE;
	int pte_idx = vpn%NR_PTES_PER_PAGE;
	
	unsigned int PFN;
	
	//pfn
	PFN = current->pagetable.outer_ptes[pd_idx]->ptes[pte_idx].pfn;
	
	//초기화
	mapcounts[PFN]--;	
	ptbr->outer_ptes[pd_idx]->ptes[pte_idx].valid = 0;
	ptbr->outer_ptes[pd_idx]->ptes[pte_idx].writable = 0;
	ptbr->outer_ptes[pd_idx]->ptes[pte_idx].pfn = 0;
	
	//tlb초기화
	for(int i=0;i<NR_TLB_ENTRIES;i++){
		if(tlb[i].vpn == vpn){
			tlb[i].valid = 0;
			tlb[i].vpn = 0;
			tlb[i].pfn = 0;
		}
	}

}


/**
 * handle_page_fault()
 *
 * DESCRIPTION
 *   Handle the page fault for accessing @vpn for @rw. This function is called
 *   by the framework when the __translate() for @vpn fails. This implies;
 *   0. page directory is invalid
 *   1. pte is invalid
 *   2. pte is not writable but @rw is for write
 *   This function should identify the situation, and do the copy-on-write if
 *   necessary.
 *
 * RETURN
 *   @true on successful fault handling
 *   @false otherwise
 */
bool handle_page_fault(unsigned int vpn, unsigned int rw)
{
	int pd_idx = vpn/NR_PTES_PER_PAGE;
	int pte_idx = vpn%NR_PTES_PER_PAGE;

	//copy on write
	if(!current->pagetable.outer_ptes[pd_idx]->ptes[pte_idx].writable && current->pagetable.outer_ptes[pd_idx]->ptes[pte_idx].private){
		//write켜기
		current->pagetable.outer_ptes[pd_idx]->ptes[pte_idx].writable = 1;
		
		unsigned PFN = current->pagetable.outer_ptes[pd_idx]->ptes[pte_idx].pfn;
		mapcounts[PFN]--;

		//다른 mapping없을때
		if(mapcounts[PFN] == 0){
			current->pagetable.outer_ptes[pd_idx]->ptes[pte_idx].private = 0;
			mapcounts[PFN]++;
			return true;
		}

		//frame에 다른 mapping도 있을때 새로운 frame찾기
		for(int i=0;i<NR_PTES_PER_PAGE;i++){
			if(!mapcounts[i]){
				current->pagetable.outer_ptes[pd_idx]->ptes[pte_idx].pfn = i;
				current->pagetable.outer_ptes[pd_idx]->ptes[pte_idx].private = 0;
				mapcounts[i]++;
				break;
			}
		}
		return true;
	}


	return false;
}


/**
 * switch_process()
 *
 * DESCRIPTION
 *   If there is a process with @pid in @processes, switch to the process.
 *   The @current process at the moment should be put into the @processes
 *   list, and @current should be replaced to the requested process.
 *   Make sure that the next process is unlinked from the @processes, and
 *   @ptbr is set properly.
 *
 *   If there is no process with @pid in the @processes list, fork a process
 *   from the @current. This implies the forked child process should have
 *   the identical page table entry 'values' to its parent's (i.e., @current)
 *   page table. 
 *   To implement the copy-on-write feature, you should manipulate the writable
 *   bit in PTE and mapcounts for shared pages. You may use pte->private for 
 *   storing some useful information :-)
 */
void switch_process(unsigned int pid)
{

	int exist = 0; //pid가 readyqueue안에 존재하면 1
	

	//current pid가 아닌경우
	if(current->pid != pid){
		struct process *tmp;
		list_for_each_entry_reverse(tmp, &processes, list){	
			//readyqueue안에 pid가 존재하면,
			if(tmp->pid == pid){
				exist = 1;
				
				struct process *tmp2 = (struct process*)malloc(sizeof(struct process));
				//copy current
				tmp2->pid = current->pid;
				
				//pte하나씩 복사
				for (int i = 0; i < NR_PTES_PER_PAGE; i++) {
					if(current->pagetable.outer_ptes[i]){
					struct pte_directory* pd = current->pagetable.outer_ptes[i];

					tmp2->pagetable.outer_ptes[i] = (struct pte_directory*)malloc(sizeof(struct pte_directory));
					for (int j = 0; j < NR_PTES_PER_PAGE; j++) {
						struct pte* pte = &pd->ptes[j];
						
						if(pte){
						tmp2->pagetable.outer_ptes[i]->ptes[j].valid = pte->valid;
						tmp2->pagetable.outer_ptes[i]->ptes[j].writable = pte->writable;
						tmp2->pagetable.outer_ptes[i]->ptes[j].pfn = pte->pfn;
						tmp2->pagetable.outer_ptes[i]->ptes[j].private = pte->private;
					}}}
				}

				tmp2->list = current->list;
				
				//current를 readyqueue에 넣기
				list_add_tail(&tmp2->list, &processes);
				//current 바꾸기
				current = tmp;
				ptbr = &tmp->pagetable;
				//switch할 process, readyqueue에서 삭제
				list_del(&tmp->list);
				
				//TLB flush
				for(int i=0;i<NR_TLB_ENTRIES;i++){
					tlb[i].valid = 0;
					tlb[i].vpn = 0;
					tlb[i].pfn = 0;
				}

				break;
			}
		}
		//readyqueue안에 pid가 존재하지 않을 때. 
		//fork()
		if(!exist)
		{
			struct process *new = (struct process*)malloc(sizeof(struct process));
			new->pid = pid;

			//pte복사
			for (int i = 0; i < NR_PTES_PER_PAGE; i++) {
				if(current->pagetable.outer_ptes[i]){
				struct pte_directory* pd = current->pagetable.outer_ptes[i];

				new->pagetable.outer_ptes[i] = (struct pte_directory*)malloc(sizeof(struct pte_directory));
				for (int j = 0; j < NR_PTES_PER_PAGE; j++) {
					struct pte* pte = &pd->ptes[j];
					if(pte){
					new->pagetable.outer_ptes[i]->ptes[j].valid = pte->valid;
					new->pagetable.outer_ptes[i]->ptes[j].writable = pte->writable;
					new->pagetable.outer_ptes[i]->ptes[j].pfn = pte->pfn;
					new->pagetable.outer_ptes[i]->ptes[j].private = pte->private;
				}}}
			}

			//write끄기 
			for(int i=0;i<NR_PTES_PER_PAGE;i++){
				for(int j=0;j<NR_PTES_PER_PAGE;j++){
					if(current->pagetable.outer_ptes[i]){
						
						if(current->pagetable.outer_ptes[i]->ptes[j].writable == 1)
							{
								current->pagetable.outer_ptes[i]->ptes[j].private = 1;
								new->pagetable.outer_ptes[i]->ptes[j].private = 1;

								current->pagetable.outer_ptes[i]->ptes[j].writable = 0;
								new->pagetable.outer_ptes[i]->ptes[j].writable = 0;
							
							}
						
					}
					
				}
				
			}

			//fork하면 current에 할당되어있던 page들 복사
			for(int i=0;i<NR_PTES_PER_PAGE;i++)
			{
				if(current->pagetable.outer_ptes[i]){
				for(int j=0;j<NR_PTES_PER_PAGE;j++){
					if(current->pagetable.outer_ptes[i]->ptes[j].valid)
					{
						int pfn = current->pagetable.outer_ptes[i]->ptes[j].pfn;
						mapcounts[pfn]++;
					}
				}}
			}
			
			//tlb flush
			for(int i=0;i<NR_TLB_ENTRIES;i++)
			{
				tlb[i].valid = 0;
				tlb[i].vpn = 0;
				tlb[i].pfn = 0;
			}
			//copy current
			struct process *tmp = (struct process*)malloc(sizeof(struct process));
			tmp->pid = current->pid;

			//pte복사
			for (int i = 0; i < NR_PTES_PER_PAGE; i++) {
				if(current->pagetable.outer_ptes[i]){
				struct pte_directory* pd = current->pagetable.outer_ptes[i];

				tmp->pagetable.outer_ptes[i] = (struct pte_directory*)malloc(sizeof(struct pte_directory));
				for (int j = 0; j < NR_PTES_PER_PAGE; j++) {
					struct pte* pte = &pd->ptes[j];
					if(pte){

					tmp->pagetable.outer_ptes[i]->ptes[j].valid = pte->valid;
					tmp->pagetable.outer_ptes[i]->ptes[j].writable = pte->writable;
					tmp->pagetable.outer_ptes[i]->ptes[j].pfn = pte->pfn;
					tmp->pagetable.outer_ptes[i]->ptes[j].private = pte->private;
				}}}
			}

			tmp->list = current->list;
			
			//current readyqueue에 집어넣기
			list_add_tail(&tmp->list, &processes);
			
			ptbr = &new->pagetable;
			//current update
			current = new;
		
		}

	}
	//current pid와 같은경우
	//아무 동작도 안한다.



}

