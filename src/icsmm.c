/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 * If you want to make helper functions, put them in helpers.c
 */
#include "icsmm.h"
#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>

#include "errno.h"

ics_free_header *freelist_head = NULL;
ics_free_header *freelist_next = NULL;

int first = 1;

void *ics_malloc(size_t size) 
{
	if (size == 0)
	{
		errno = EINVAL;
		return NULL;
	}

	else if (size > 16352)
	{
		errno = ENOMEM;
		return NULL;
	}

	// First time calling malloc
	if (first)
	{
		first = 0;
		// add prologue and epilogue
		uint64_t *prologue = (uint64_t *)ics_inc_brk();
		if ((void *)prologue == (void *)-1)
			return NULL;
		*prologue = 1;
		uint64_t *epilogue = prologue + (4096 / 8) - 1;
		*epilogue = 1;
		// make a huge free block
		ics_free_header *empty = (ics_free_header *)(prologue + 1);
		(empty->header).block_size = 4080;
		(empty->header).unused = 0xaaaaaaaa;
		(empty->header).requested_size = 0;
		empty->next = NULL;
		empty->prev = NULL;

		ics_footer *emptyf = (ics_footer *)(epilogue -1);
		emptyf->block_size = 4080;
		emptyf->unused = 0xffffffffffff;

		freelist_head = empty;
		freelist_next = empty;
	}
	size_t blockSize = 16 * ((size + 16 + 15) / 16);


	ics_free_header *start = freelist_next;
	while (1)
	{	// empty free list
		if (freelist_next == NULL)
		{
			uint64_t *newPage = (uint64_t *)ics_inc_brk();
			if ((void *)newPage == (void *)-1)
				return NULL;
			uint64_t *newEpilogue = newPage + (4096 / 8) - 1;
			*newEpilogue = 1;
			ics_free_header *newFreeH = (ics_free_header *)(newPage - 1);
			(newFreeH->header).block_size = 4096;
			(newFreeH->header).unused = 0xaaaaaaaa;
			(newFreeH->header).requested_size = 0;
			newFreeH->prev = NULL;
			newFreeH->next = NULL;
			
			ics_footer *newFreeF = (ics_footer *)(newEpilogue - 1);
			newFreeF->block_size = 4096;
			newFreeF->unused = 0xffffffffffff;

			freelist_head = newFreeH;
			freelist_next = newFreeH;
		}

		else
		{
			if ((freelist_next->header).block_size >= blockSize)
				break;

			else
			{
				if (start == NULL)
					start = freelist_next;

				freelist_next = freelist_next->next;
				if (freelist_next == NULL)
					freelist_next = freelist_head;
				if (freelist_next == start)
				{
					uint64_t *np = (uint64_t *)ics_inc_brk();
					if ((void *)np == (void *)-1)
						return NULL;
					uint64_t *epi = np + (4096 / 8) - 1;
					*epi = 1;

					ics_footer *curfter = (ics_footer *)(epi - 1);
					curfter->unused = 0xffffffffffff;

					ics_footer *prevfter = (ics_footer *)(np - 2);
					// prev block is free
					if ((prevfter->block_size) % 2 == 0)
					{
						int bsize = prevfter->block_size;
						ics_free_header *prevhder = (ics_free_header *)(prevfter - bsize / 8 + 1);
						(prevhder->header).block_size = bsize + 4096;
						curfter->block_size = bsize + 4096;
						
						if (prevhder->prev == NULL && prevhder->next == NULL)
						{}
						else if (prevhder->prev == NULL && prevhder->next != NULL)
						{}
						else if (prevhder->prev != NULL && prevhder->next == NULL)
						{
							(prevhder->prev)->next = NULL;
							prevhder->prev = NULL;
							prevhder->next = freelist_head;
							freelist_head->prev = prevhder;
						}
						else if (prevhder->prev != NULL && prevhder->next != NULL)
						{
							(prevhder->prev)->next = prevhder->next;
							(prevhder->next)->prev = prevhder->prev;
							prevhder->prev = NULL;
							prevhder->next = freelist_head;
							freelist_head->prev = prevhder;
						}

						freelist_head = prevhder;
						freelist_next = prevhder;
						start = freelist_next;
					}
					// prev block is allocated
					else
					{
						ics_free_header *curhder = (ics_free_header *)(np - 1);
						(curhder->header).block_size = 4096;
						(curhder->header).unused = 0xaaaaaaaa;
						(curhder->header).requested_size = 0;
						curfter->block_size = 4096;

						curhder->prev = NULL;
						curhder->next = freelist_head;
						if (freelist_head != NULL)
							freelist_head->prev = curhder;
						freelist_head = curhder;
						freelist_next = curhder;
						start = freelist_next;
					}
				}
			}
		}
	}
	
	
	// change freelist_next and remove selected free block
	ics_free_header *current = freelist_next;
	freelist_next = freelist_next->next;

	if (current->prev == NULL && current->next == NULL)
		freelist_head = NULL;
	else if (current->prev == NULL && current->next != NULL)
	{
		freelist_head = freelist_next;
		freelist_next->prev = NULL;
	}
	else if (current->prev != NULL && current->next == NULL)
	{
		(current->prev)->next = NULL;
		freelist_next = freelist_head;
	}
	else
	{
		(current->prev)->next = current->next;
		(current->next)->prev = current->prev;
	}



	int split = 1;
	int freeRemain = (current->header).block_size - blockSize;

	ics_header *hder = (ics_header *)current;
	
	if (freeRemain < 32)
	{
		blockSize = blockSize + freeRemain;
		split = 0;
	}

	// allocate the block
	hder->block_size = blockSize + 1;
	hder->unused = 0xaaaaaaaa;
	hder->requested_size = size;

	ics_footer *fter = (ics_footer *)(hder + blockSize/8 - 1);
	fter->block_size = blockSize + 1;
	fter->unused = 0xffffffffffff;

	// split the block and add the new free block to freelist_head
	if (split)
	{
		ics_free_header *fhder = (ics_free_header *)(hder + blockSize/8);
		(fhder->header).block_size = freeRemain;
		(fhder->header).unused = 0xaaaaaaaa;
		(fhder->header).requested_size = 0;

		ics_footer *ffter = (ics_footer *)(((ics_header *)fhder) + freeRemain/8 -1);
		ffter->block_size = freeRemain;
		ffter->unused = 0xffffffffffff;

		// add to head of free list
		if (freelist_head == NULL)
		{
			freelist_head = fhder;
			freelist_next = fhder;
			fhder->prev = NULL;
			fhder->next = NULL;
		}
		else
		{
			freelist_head->prev = fhder;
			fhder->prev = NULL;
			fhder->next = freelist_head;
			freelist_head = fhder;
		}
	}
	return (void *)(hder + 1);
	
}


void *ics_realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
	{
		errno = EINVAL;
		return NULL;
	}
	// check if ptr is between prologue and epilogue
	uint64_t *highest = (uint64_t *)ics_get_brk();
	uint64_t *lowest = highest - 4096/8;
	while (*lowest != 1)
		lowest = lowest - 4096/8;
	if (ptr < (void *)lowest || ptr > (void *)highest)
	{
		errno = EINVAL;
		return NULL;
	}

	ics_header *hder = (ics_header *)(ptr - 8);
	int hdersize = hder->block_size;
	
	if (hder->unused != 0xaaaaaaaa || (hdersize % 2 == 0) || (hdersize < 33) || ((hdersize-1) % 8 != 0))
	{
		errno = EINVAL;
		return NULL;
	}

	ics_footer *fter = (ics_footer *)(hder + (hdersize-1) / 8 - 1);
	
	if ((fter->unused != 0xffffffffffff) || (fter->block_size != hdersize))
	{
		errno = EINVAL;
		return NULL;
	}

	if (size == 0)
	{
		ics_free(ptr);
		return NULL;
	}
	
	size_t oldsize = hder->requested_size;
	size_t cpysize;
	if (oldsize < size)
		cpysize = oldsize;
	else
		cpysize = size;

	uint64_t *newptr = ics_malloc(size);
	if ((void *)newptr == (void *)-1)
		return NULL;

	char *opayload = (char *)(hder + 1);
	char *npayload = (char *)(newptr + 1);

	int i;
	for (i = 0; i < cpysize; i++)
		*(npayload + i) = *(opayload + i);
	
	return (void *)npayload;
}


int ics_free(void *ptr)
{
	if (ptr == NULL)
	{
		errno = EINVAL;
		return -1;
	}
	// check if ptr is between prologue and epilogue
	uint64_t *highest = (uint64_t *)ics_get_brk();
	uint64_t *lowest = highest - 4096/8;
	while (*lowest != 1)
		lowest = lowest - 4096/8;
	if (ptr < (void *)lowest || ptr > (void *)highest)
	{
		errno = EINVAL;
		return -1;
	}

	ics_header *hder = (ics_header *)(ptr - 8);
	int hdersize = hder->block_size;
	
	if (hder->unused != 0xaaaaaaaa || (hdersize % 2 == 0) || (hdersize < 33) || ((hdersize-1) % 8 != 0))
	{
		errno = EINVAL;
		return -1;
	}

	ics_footer *fter = (ics_footer *)(hder + (hdersize-1) / 8 - 1);
	
	if ((fter->unused != 0xffffffffffff) || (fter->block_size != hdersize))
	{
		errno = EINVAL;
		return -1;
	}

	int next_in_coalesced = 0;

	// set allocated bit to 0
	hder->block_size = hder->block_size - 1;
	fter->block_size = fter->block_size - 1;

	ics_header *nexthder = (ics_header *)(fter + 1);
	ics_footer *prevfter = (ics_footer *)(hder - 1);


	//case 1
	if (nexthder->block_size % 2 == 1 && prevfter->block_size % 2 == 1)
	{
		ics_free_header *fhder = (ics_free_header *)hder;
		fhder->prev = NULL;
		fhder->next = freelist_head;
		if (freelist_head == NULL)
			freelist_next = fhder;
		else
			freelist_head->prev = fhder;
		freelist_head = fhder;
	}
	//case 2
	else if (nexthder->block_size % 2 == 0 && prevfter->block_size % 2 == 1)
	{
		ics_free_header *fhder = (ics_free_header *)hder;
		fhder->prev = NULL;
		fhder->next = freelist_head;
		if (nexthder == (ics_header *)freelist_next)
			next_in_coalesced = 1;
		hder->block_size = hder->block_size + nexthder->block_size;
		ics_footer *ffter = (ics_footer *)(hder + (hder->block_size)/8 - 1);
		ffter->block_size = hder->block_size;

		ics_free_header *nhder = (ics_free_header *)nexthder;
		if (nhder->prev == NULL && nhder->next == NULL)
		{}
		else if (nhder->prev == NULL && nhder->next != NULL)
			(freelist_head->next)->prev = fhder;
		else if (nhder->prev != NULL && nhder->next == NULL)
		{
			(nhder->prev)->next = NULL;
			freelist_head->prev = fhder;
		}
		else
		{
			(nhder->prev)->next = nhder->next;
			(nhder->next)->prev = nhder->prev;
			freelist_head->prev = fhder;
		}
		freelist_head = fhder;
		if (next_in_coalesced)
			freelist_next = freelist_head;
	}
	//case 3
	else if (nexthder->block_size % 2 == 1 && prevfter->block_size % 2 == 0)
	{
		int bsize = prevfter->block_size;
		ics_free_header *fhder = (ics_free_header *)(prevfter - (prevfter->block_size)/8 + 1);
		(fhder->header).block_size = bsize + hder->block_size;
		fter->block_size = bsize + hder->block_size;

		if (fhder == freelist_next)
			next_in_coalesced = 1;

		if (fhder->prev == NULL && fhder->next == NULL)
		{}
		else if (fhder->prev == NULL && fhder->next != NULL)
		{}
		else if (fhder->prev != NULL && fhder->next == NULL)
		{
			(fhder->prev)->next = NULL;
			fhder->prev = NULL;
			fhder->next = freelist_head;
			freelist_head->prev = fhder;
		}
		else
		{
			(fhder->prev)->next = fhder->next;
			(fhder->next)->prev = fhder->prev;
			fhder->prev = NULL;
			fhder->next = freelist_head;
			freelist_head->prev = fhder;
		}
		freelist_head = fhder;
		if (next_in_coalesced)
			freelist_next = freelist_head;
	}
	//case 4
	else if (nexthder->block_size % 2 == 0 && prevfter->block_size % 2 == 0)
	{
		int total = nexthder->block_size + hder->block_size + prevfter->block_size;
		ics_free_header *fhder = (ics_free_header *)(prevfter - (prevfter->block_size)/8 + 1);
		ics_footer *ffter = (ics_footer *)(nexthder + (nexthder->block_size)/8 -1);
		(fhder->header).block_size = total;
		ffter->block_size = total;
		
		ics_free_header *nhder = (ics_free_header *)nexthder;
		if (fhder == freelist_next || nexthder == (ics_header *)freelist_next)
			next_in_coalesced = 1;

		if (nhder->prev == NULL && nhder->next != NULL)
		{
			(nhder->next)->prev = NULL;
			freelist_head = freelist_head->next;
		}
		else if (nhder->prev != NULL && nhder->next == NULL)
			(nhder->prev)->next = NULL;
		else if (nhder->prev != NULL && nhder->next != NULL)
		{
			(nhder->prev)->next = nhder->next;
			(nhder->next)->prev = nhder->prev;
		}

		if (fhder->prev == NULL && fhder->next == NULL)
		{}
		else if (fhder->prev == NULL && fhder->next != NULL)
		{}
		else if (fhder->prev != NULL && fhder->next == NULL)
		{
			(fhder->prev)->next = NULL;
			fhder->prev = NULL;
			fhder->next = freelist_head;
			freelist_head->prev = fhder;
		}
		else if (fhder->prev != NULL && fhder->next != NULL)
		{
			(fhder->prev)->next = fhder->next;
			(fhder->next)->prev = fhder->prev;
			fhder->prev = NULL;
			fhder->next = freelist_head;
			freelist_head->prev = fhder;
		}
		freelist_head = fhder;
		if (next_in_coalesced)
			freelist_next = freelist_head;

	}
	return 0;
}
