/*! \file
 * Implementation of a simple memory allocator.  The allocator manages a small
 * pool of memory, provides memory chunks on request, and reintegrates freed
 * memory back into the pool.
 *
 * Adapted from Andre DeHon's CS24 2004, 2006 material.
 * Copyright (C) California Institute of Technology, 2004-2010.
 * All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include "myalloc.h"

int * get_header(int *footer);
int * best_fit_block(int size);
int * get_footer(int *header);
int * get_next_header(int *header);
int * back_coalesce(int * header);
void fwd_coalesce(int * header);

/*!
 * These variables are used to specify the size and address of the memory pool
 * that the simple allocator works against.  The memory pool is allocated within
 * init_myalloc(), and then myalloc() and free() work against this pool of
 * memory that mem points to.
 */

int MEMORY_SIZE;
/* Always points to beginning of the entire memory pool. */
unsigned char *mem;
/* Always points to the end of the entire memory pool. */
static int *end;

/*!
 * This function initializes both the allocator state, and the memory pool.  It
 * must be called before myalloc() or myfree() will work at all.
 *
 * Note that we allocate the entire memory pool using malloc().  This is so we
 * can create different memory-pool sizes for testing.  Obviously, in a real
 * allocator, this memory pool would either be a fixed memory region, or the
 * allocator would request a memory region from the operating system (see the
 * C standard function sbrk(), for example).
 */

void init_myalloc() {
    /*
     * Allocate the entire memory pool, from which our simple allocator will
     * serve allocation requests.
     */
    mem = (unsigned char *) malloc(MEMORY_SIZE);
    if (mem == 0) {
        fprintf(stderr,
                "init_myalloc: could not get %d bytes from the system\n",
                MEMORY_SIZE);
        abort();
    }
    unsigned char *init = mem;
    /* Set the header size of entire memory block. */
    *((int *) init) = MEMORY_SIZE;
    /* Move to the footer region. */
    init = init + MEMORY_SIZE - sizeof(int);
    /* Set the footer size of entire memory block. */
    *((int *) init) = MEMORY_SIZE;
    /* Move to the end of the entire memory. */
    init = init + sizeof(int);
    /* Set pointer to end for use in comparison later. */
    end = (int *) init;
}
/*
 * Takes an int pointer to a footer of a block memory and returns a pointer
 * to the block's header.
 */
int * get_header(int *footer) {
    int size;
    size = abs(*footer);
    footer = (int *) ((unsigned char *) footer - size + sizeof(int));
    return footer;
}
/*
 * Returns an int pointer to the header of the smallest sized block of
 * memory which can satisfy allocation of a certain memory size. This operation
 * requires linear time with the number of memory blocks because it requires
 * looking at the size and state of allocation of each block in the entire
 * memory pool.
 */
int * best_fit_block(int size) {
    int *result = 0;
    /* Set a pointer to the beginning of the entire memory region. */
    int *header = (int *) mem;
    /* Exit the while loop when all headers have been looked through. */
    while (header != 0) {
        /* Check that size is sufficient and block is free. */
        if ((abs(*header) >= size + 8) && (*header > 0)) {
            /*
             * If result is 0, any header should replace it; otherwise result
             * should only be replaced if a smaller block is found.
             */
            if (!result || (abs(*header) < abs(*result))) {
                result = header;
            }
        }
        header = get_next_header(header);
    }
    return result;
}
/*
 * Takes an int pointer to a header of a block memory and returns a pointer
 * to the block's footer.
 */
int * get_footer(int *header) {
    int size;
    size = abs(*header);
    header = (int *) ((unsigned char *) header + size - sizeof(int));
    return header;
}
/*
 * Takes an int pointer to a header of a block memory and returns a pointer
 * to the next block's header. If there is no next block, 0 is returned.
 */
int * get_next_header(int *header) {
    int size;
    size = abs(*header);
    header = (int *) ((unsigned char *) header + size);
    /* Check if there is another header, or if end has been reached. */
    if (header >= end) {
        return 0x0;
    }
    return header;
}
/*!
 * Attempt to allocate a chunk of memory of "size" bytes.  Return 0 if
 * allocation fails. Allocation requires linear time because it necessitates
 * finding a best-fit block, which requires linear time (as described in the
 * comments above the best-fit function.)
 */
unsigned char *myalloc(int size) {
    int *header;
    unsigned char *result;
    /* Follow a best-fit strategy to find a memory block to allocate. */
    header = best_fit_block(size);

    if (header == 0) {
        fprintf(stderr, "myalloc: cannot service request of size %d\n", size);
        return 0;
    }
    else {

        int *old_header;
        old_header = header;
        /* Only split if there is enough extra space for footers and headers. */
        if (abs(*header) >= size + 4 * sizeof(int)) {

            int orig_size = abs(*header);
            /* Set allocated header size; negative since it is allocated. */
            *header = (size + 2 * sizeof(int)) * -1;
            header = get_footer(header);
            /* Set allocated footer size; negative since it is allocated. */
            *header = (size + 2 * sizeof(int)) * -1;
            /* Move to the header in the second block after the split. */
            header += 1;
            /* Set header size. */
            *header = orig_size - (size + 2 * sizeof(int));
            header = get_footer(header);
            /* Set footer size. */
            *header = orig_size - (size + 2 * sizeof(int));
        }
        else {
            /* If we don't split, just make header and footer negative. */
            *header *= -1;
            header = get_footer(header);
            *header *= -1;
        }
        result = (unsigned char *) old_header;
        /* Return a pointer to the payload. */
        return result + sizeof(int);
    }
}

/*
 * When a block is freed, this coalesces it with the block to its left, which
 * has already been guaranteed to exist and be free.
 */
 int * back_coalesce(int * header) {
     /* Size of the just freed block. */
     int right = *header;
     header -= 1;
     /* Size of the block to the left. */
     int left = *header;
     /* Move to header of the coalesced block to set size. */
     header = get_header(header);
     *header += right;
     /* Move to footer of the coalesced block to set size. */
     header = get_footer(header);
     *header += left;
     /* Set pointer in correct position before backward coalescing. */
     return get_header(header);
 }
 /*
  * When a block is freed, this coalesces it with the block to its right, which
  * has already been guaranteed to exist and be free.
  */
 void fwd_coalesce(int * header) {
     /* Size of the current block. */
     int left = *header;
     header = get_next_header(header);
     /* Size of the block to the right. */
     int right = *header;
     header -= 1;
     /* Move to the header of the coalesced block and set size. */
     header = get_header(header);
     *header += right;
     /* Move to the footer of the coalesced block and set size. */
     header = get_footer(header);
     *header += left;
 }
 /*!
  * Free a previously allocated pointer. oldptr should be an address returned by
  * myalloc().  Deallocation is a constant time operation because the most time
  * complex operation invoked is coalescing. Coalescing requires constant time
  * because it necessitates looking only at the blocks directly to the left and
  * right of the just-freed block.
  */
 void myfree(unsigned char *oldptr) {

    int *newptr;
    /* Designate header of block as freed. */
    newptr = (int *) oldptr - 1;
    *newptr *= -1;
    /* Designate footer of block as freed. */
    newptr = get_footer(newptr);
    *newptr *= -1;
    /* Set-up pointer for coalescing. */
    newptr = get_header(newptr);
    /* Check that there is a block to the left and it is free. */
    if (newptr != (int *) mem) {
        if (*(newptr - 1) > 0) {
            newptr = back_coalesce(newptr);
        }
    }
    /* Set-up pointer for coalescing. */
    newptr = get_next_header(newptr);
    /* Check that there is a block to the right and it is free. */
    if (newptr != 0) {
        if (*newptr > 0) {
            /* Move back to header to prepare for coalescing. */
            newptr -= 1;
            newptr = get_header(newptr);
            fwd_coalesce(newptr);
        }
    }
}

/*!
 * Clean up the allocator state.
 * All this really has to do is free the user memory pool. This function mostly
 * ensures that the test program doesn't leak memory, so it's easy to check
 * if the allocator does.
 */
void close_myalloc() {
    free(mem);
}
