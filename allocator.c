/**
 * @file allocator.c
 *
 * Explores memory management at the C runtime level.
 *
 * To use (one specific command):
 * LD_PRELOAD=$(pwd)/allocator.so command
 * ('command' will run with your allocator)
 *
 * To use (all following commands):
 * export LD_PRELOAD=$(pwd)/allocator.so
 * (Everything after this point will use your custom allocator -- be careful!)
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include "allocator.h"
#include "debug.h"
#include <stddef.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * Function that will split block if we need to
 *
 * @param ptr
 * @param size
 */
void *split(struct mem_block *ptr, size_t size)
{
	if (ptr->usage == 0)
	{
		ptr->alloc_id =g_allocations++;
		ptr->usage = size;
		LOG("returning pointer at %p\n", ptr);
		return ptr;
	}
	else
	{
		struct mem_block *new = (void *)ptr + ptr->usage;
		LOGP("created a split new block\n");
		new->alloc_id = g_allocations++;
		LOG("size and usage %lu %lu\n", ptr->size, ptr->usage);	
		new->size = ptr->size - ptr->usage;
		new->usage = size;
		LOGP("WE OUT HERE\n");
		new->region_start = ptr->region_start;
		new->region_size = ptr->region_size;
	        LOGP("MADE IT HERE\n");
		ptr->size = ptr->usage;
		new->next = ptr->next;
		ptr->next = new;
		LOG("split at %p\n", new);
		return new;
	}
}

/**
 * print_memory
 *
 * Prints out the current memory state, including both the regions and blocks.
 * Entries are printed in order, so there is an implied link from the topmost
 * entry to the next, and so on.
 */
void print_memory(void)
{
    puts("-- Current Memory State --");
    struct mem_block *current_block = g_head;
    struct mem_block *current_region = NULL;
    while (current_block != NULL) {
        if (current_block->region_start != current_region) {
            current_region = current_block->region_start;
            printf("[REGION] %p-%p %zu\n",
                    current_region,
                    (void *) current_region + current_region->region_size,
                    current_region->region_size);
        }
        printf("[BLOCK]  %p-%p (%lu) '%s' %zu %zu %zu\n",
                current_block,
                (void *) current_block + current_block->size,
                current_block->alloc_id,
                current_block->name,
                current_block->size,
                current_block->usage,
                current_block->usage == 0
                    ? 0 : current_block->usage - sizeof(struct mem_block));
        current_block = current_block->next;
    }

}
/**
 * Function that will find the first available spot 
 * in our memory
 *
 * @param size 			
 */
void *firstFit(size_t size)
{
	struct mem_block *iter = g_head;

	LOG("IN FIRST iter is %p\n", iter); 
	while (iter != NULL)
	{
		if ((iter->size - iter->usage) >= size)
		{
			LOG("first completed at %p\n", iter);
			return iter;
		}
		iter = iter->next;
	}

	LOGP("UH OH");
	
	return NULL;
}

/**
 * function that will find the best possible scenario 
 * to add the block
 *
 * @param size
 */
void *bestFit(size_t size)
{
	struct mem_block *iter = g_head;
	struct mem_block *best = NULL;

	size_t min = SIZE_MAX;
	while (iter != NULL)
	{
		if ((iter->size - iter->usage) >= size)
		{
			if ((iter->size - iter->usage) == size)
			{
				return iter;
			}
			else if ((iter->size - iter->usage) < min)
			{
				best = iter;
				min = iter->size - iter->usage;
			}
		}	
		iter = iter->next;
	}
	
	LOG("best completed at %p", best);
	return best;
}

/**
 * function to return the worst possible position to add a pointer.
 *
 * @param size
 */
void *worstFit(size_t size)
{
	struct mem_block *worst = NULL;
	size_t max = 0;

	struct mem_block *iter = g_head;
	
	 
	while (iter != NULL)
	{
		size_t available = iter->size - iter->usage;
		if (available >= size)
		{
			if (available > max)
			{
				worst = iter;
				max = available;
			}
		}
		iter = iter->next;
	}
	LOG("worst completed at %p", worst);
	return worst;


}
/**
 * fucntion to see whether we reuse and will 
 * return correct pointer in case we do. 
 *
 * @param size 
 */
void *reuse(size_t size)
{
   if (g_head == NULL)
   {
	return NULL;
   } 

   char * algo = getenv("ALLOCATOR_ALGORITHM");
   if (algo == NULL)
   {
	algo = "first_fit";
   }

   LOG("Algo used %s\n", algo);
   struct mem_block *returnMe = NULL;

   if (strcmp(algo, "first_fit") == 0)
   {
	returnMe = firstFit(size);
   }
   else if (strcmp(algo, "best_fit") == 0)
   {
	returnMe = bestFit(size);
   }
   else if (strcmp(algo, "worst_fit") == 0)
   {
	returnMe = worstFit(size);
   }

   if (returnMe == NULL)
   {
	return NULL;
   }
   struct mem_block * reused = split(returnMe, size);

   LOG("reused is %p \n", reused);
   return reused;
}

/**
 * Function to scribble memory
 *
 * @param ptr		ptr to scramble
 * @param size 		size of pointer to scramble
 */
void scribbler(void * ptr, size_t size)
{
	memset(ptr, 0xAA, size);
}

/**
 * Function that replaces our malloc function 
 * takes in size parameter. 
 *
 * @param size 
 */
void *malloc(size_t size)
{
    pthread_mutex_lock(&lock);
    if (size % 8 != 0)
    {
	size = size + size % 8;
    }
 
    size_t real_size = size + sizeof(struct mem_block);
    struct mem_block *reusable = reuse(real_size);

    char * scrib_check = getenv("ALLOCATOR_SCRIBBLE");
    int scribble = 0;

    if (scrib_check != NULL)
    {
	scribble = atoi(scrib_check);		
    }

    
   if (reusable != NULL)
   {
	if (scribble)
	{
		scribbler(reusable+1, size);
	}
	pthread_mutex_unlock(&lock);
	return reusable + 1;

   }

    
       
    int page_size = getpagesize();
    size_t num_pages = real_size / page_size;
    if (real_size % page_size != 0)
    {
	    num_pages++;
    } 
    size_t region_size = num_pages * page_size;    
    LOG("allocating for size %lu on for allocation number %lu real size %lu num pages %lu\n", region_size, g_allocations, real_size, num_pages);

    struct mem_block *block = (struct mem_block *) mmap(NULL, 
		    					region_size, 
							PROT_READ | PROT_WRITE, 
							MAP_PRIVATE | MAP_ANONYMOUS, 
							-1, 
							0);


    if (block == MAP_FAILED)
    {
	    perror("mmap");
	    pthread_mutex_unlock(&lock);
	    return NULL;
    }

    block->alloc_id = g_allocations++; 
    strcpy(block->name, "hoi");
    block->size = region_size; 
    block->usage = real_size;
    block->region_start = block; 
    block->region_size = region_size;
    block->next = NULL;  


    if (g_head == NULL)
    {
	g_head = block;
    }
    else
    {
	    struct mem_block *iter = g_head;
	    while (iter != NULL)
	    {
		if (iter->next == NULL)
		{
			iter->next = block; 
			break;
		}
		iter = iter->next;
	    }	
    }
 

   if (scribble != 0)
   {
	scribbler(block + 1, size);
   }
    pthread_mutex_unlock(&lock);
    return block + 1;
}

/**
 * will malloc memory and add a name to that memory block 
 *
 * @param size 
 * @param name
 */
void *malloc_name(size_t size, char *name)
{
    LOG("Allocation request with size = %zu\n", size);
    struct mem_block *ptr = malloc(size);
    strcpy((ptr-1)->name, name);

    return ptr;
}



/**
 * function that will write memory
 *
 * @param p 
 */
void write_memory(FILE * p)
{
    fputs("-- Current Memory State --", p);
    struct mem_block *current_block = g_head;
    struct mem_block *current_region = NULL;
    while (current_block != NULL) {
        if (current_block->region_start != current_region) {
            current_region = current_block->region_start;
            fprintf(p, "[REGION] %p-%p %zu\n",
                    current_region,
                    (void *) current_region + current_region->region_size,
                    current_region->region_size);
        }
        fprintf(p, "[BLOCK]  %p-%p (%lu) '%s' %zu %zu %zu\n",
                current_block,
                (void *) current_block + current_block->size,
                current_block->alloc_id,
                current_block->name,
                current_block->size,
                current_block->usage,
                current_block->usage == 0
                    ? 0 : current_block->usage - sizeof(struct mem_block));
        current_block = current_block->next;
    }
	
}

/**
 * Function to free current block or remove entire block of code if empty. 
 *
 * @param ptr 			ptr to remove
 */
void free(void *ptr)
{
	
    if (ptr == NULL) 
    {
        return;
    }
    pthread_mutex_lock(&lock);
    LOG("Free request at %p\n", ptr);

    struct mem_block *block = (struct mem_block *) ptr - 1;
    LOGP("created new block\n");
    block->usage = 0;

    struct mem_block *iter = block->region_start;
    LOG("mem iter %p\n", iter);

    struct mem_block *next = iter->next;

    LOG("starting at %p\n", block->region_start);
   
   
    while (iter != NULL && iter->region_start == block->region_start )
    {
	if (iter->usage != 0)
	{
		pthread_mutex_unlock(&lock);
		return; 
	}
	next = iter->next;
	iter = iter->next;
    }

    LOG("next pointer = %p\n", next);
    iter = g_head; 
    struct mem_block *prev = NULL;
   
    if (iter == block->region_start)
    {
	g_head = next;
    }
    else
    {
    	while (iter->region_start != block->region_start)
    	{
		if (iter->next->region_start == block->region_start)
		{
			prev = iter;
			prev->next = next;
			break;
		}
		LOGP("STUCK");
		iter = iter->next;
    	} 
    }

    LOG("Freeing block: %s, region size: %zu\n", block->name, block->region_size);
    int ret = munmap(block->region_start, block->region_size);

    if (ret == -1)
    {
	pthread_mutex_unlock(&lock);
	perror("munmap");
    }


    
    pthread_mutex_unlock(&lock);
 
}

/**
 * Function that replaces normal calloc
 *
 * @param nmemb
 * @param size
 */
void *calloc(size_t nmemb, size_t size)
{
    if (size * nmemb == 0)
    {
	return NULL;
    }
    void *block = malloc(nmemb * size);
    pthread_mutex_lock(&lock);
    //scribbler(block + 1, nmemb * size);
    memset(block, 0, nmemb * size);
    pthread_mutex_unlock(&lock);
    return block;

}

/**
 * Function that will reallocate memory! 
 *
 * @param ptr 
 * @param size 
 */
void *realloc(void *ptr, size_t size)
{
    size = size + size % 8;
    size = size + sizeof(struct mem_block);

    if (ptr == NULL) {
        /* If the pointer is NULL, then we simply malloc a new block */
        return malloc(size);
    }

    if (size == 0) {
        /* Realloc to 0 is often the same as freeing the memory block... But the
         * C standard doesn't require this. We will free the block and return
         * NULL here. */
        free(ptr);
        return NULL;
    }


    struct mem_block *old = (struct mem_block *) ptr - 1;

    if (old->size < size)
    {
	void *new = malloc(size);
	if (new == NULL) 
	{
		return NULL;
	}
    
        pthread_mutex_lock(&lock);
        memcpy(new, ptr, old->usage);
        pthread_mutex_unlock(&lock);
	free(ptr);

	return new;
    }
    else
    {
	old->usage = size;
	return ptr;
    }
	
    return NULL;
}

