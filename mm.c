/*
 * mm.c - An explicit list using a binary tree.
 * Here, we don't have a fancy segregated list of different sizes,
 * but I utilized the BT structure in order to order
 * the blocks in the free list, so that the lookup time could be
 * minimized.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "Skylar",
    /* First member's full name */
    "Skylar Nam",
    /* First member's WUSTL key */
    "435454",
    /* Second member's full name (leave blank) */
    "",
    /* Second member's WUSTL key (leave blank) */
    ""
};

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8 

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* macros from the textbook */

/* Basic constants and macros */
#define WSIZE       	4       /* Word and header/footer size (bytes) */
#define DSIZE       	8       /* Double word size (bytes) */
#define CHUNKSIZE  		(1<<12)  /* Extend heap by this amount (bytes) */
#define NODESIZE		24
#define INISIZE 		1016

#define MAX(x, y) 		((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       	(*(unsigned int *)(p))
#define PUT(p, val)  	(*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  	(GET(p) & ~0x7)
#define GET_ALLOC(p) 	(GET(p) & 0x1)

/* Read the size and allocated fields from block pointer bp and
 * return the pointer */
#define SIZE_PTR(bp) 	(GET(HDRP(bp)) & ~0x7)
#define ALLOC_PTR(bp) 	(GET(HDRP(bp)) & 0x1)

/* Given block ptr bp, compute address of its header, footer,
 * left, right, parent, and its siblings
 *   _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
 *  |hdr | left | right | parent | siblings | content | ftr |
 *   - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 */
#define HDRP(bp)		((char *)(bp) - WSIZE)
#define FTRP(bp)		((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define LEFT(bp)		((char *)(bp))
#define RIGHT(bp)		((char *)(bp) + WSIZE)
#define PARENT(bp)		((char *)(bp) + 2*WSIZE)
#define SIBLINGS(bp)	((char *)(bp) + 3*WSIZE)

/* Given block ptr bp, compute address of next and previous blocks 
 * as well as the header, footer, left, right, parent, and the siblings */
// #define NEXT_BLKP(bp)	((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define NEXT_BLKP(bp) 	  ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)	  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define HDRP_BLKP(bp)	  (GET(HDRP(bp)))
#define FTRP_BLKP(bp)	  (GET(FTRP(bp)))
#define LEFT_BLKP(bp)	  (GET(LEFT(bp)))
#define RIGHT_BLKP(bp)	  (GET(RIGHT(bp)))
#define PARENT_BLKP(bp)	  (GET(PARENT(bp)))
#define SIBLINGS_BLKP(bp) (GET(SIBLINGS(bp)))

/* Assign value to each */
#define PUT_HDRP(bp, val) 		(PUT(HDRP(bp), (int)val))
#define PUT_FTRP(bp, val) 		(PUT(FTRP(bp), (int)val))
#define PUT_LEFT(bp, val) 		(PUT(LEFT(bp), (int)val))
#define PUT_RIGHT(bp, val) 		(PUT(RIGHT(bp), (int)val))
#define PUT_PARENT(bp, val) 	(PUT(PARENT(bp), (int)val))
#define PUT_SIBLINGS(bp, val) 	(PUT(SIBLINGS(bp), (int)val))

static char *heap_listp;
static char *free_listp;

static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);

static void insert_node(void *bp);
static void remove_node(void *bp);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) return -1;

	/* Alignment padding */
	PUT(heap_listp, 0);
	/* Prologue header */
	PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
	/* Prologue footer */
	PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
	/* Epilogue header */
	PUT(heap_listp + (3*WSIZE), PACK(0, 1));
	heap_listp += (2*WSIZE);
	
	/* Extend the empty heap with a free block of CHUNKSIZE bytes */
	if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
	
    return 0;
}

static void *extend_heap(size_t words)
{
	char *bp;
	size_t size;
	
	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	if ((long)(bp = mem_sbrk(size)) == -1) return NULL;
	
	/* Initialize free block header/footer and the epilogue header */
	/* Free block header */
	PUT(HDRP(bp), PACK(size, 0));
	/* Free block footer */
	PUT(FTRP(bp), PACK(size, 0));
	/* New epilogue header */
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

	/* Coalesce if the previous block was free */
	return coalesce(bp);	
}

static void *find_fit(size_t asize)
{
	void *bp;

	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
			return bp;
		}
	}

	/* no fit */
	return NULL;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	char *bp;
	/* Adjusted block size */
	size_t asize;
	/* Amount to extend heap if no fit */
	size_t extendsize;
	
	/* Ignore spurious requests */
	if (size == 0) return NULL;
	
	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE) asize = 2*DSIZE;
	else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
	
	/* Search the free list for a fit */
	if ((bp = find_fit(asize)) != NULL) {
		place(bp, asize);
		return bp;
	}
	
	/* No fit found. Get more memory and place the block */
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
	place(bp, asize);
	return bp;
}

/*
 * place - Place the requested block at the beginning of the free block, 
 * splitting only if the size of the remainder would equal or exceed the 
 * minimum block size.
 */
static void place(void *bp, size_t asize)
{
	size_t size = GET_SIZE(HDRP(bp));

	if ((size - asize) >= (WSIZE*4)) {
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		PUT(HDRP(NEXT_BLKP(bp)), PACK(size-asize, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size-asize, 0));
		
		bp = NEXT_BLKP(bp);
	}
	else {
		PUT(HDRP(bp), PACK(size, 1));
		PUT(FTRP(bp), PACK(size, 1));
	}
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
	size_t size = GET_SIZE(HDRP(bp));
	
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	
	coalesce(bp);
}

static void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	/* Case 1 */
	if (prev_alloc && next_alloc) {
    	return bp;
	}

	/* Case 2 */
	else if (prev_alloc && !next_alloc) {
    	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    	PUT(HDRP(bp), PACK(size, 0));
    	PUT(FTRP(bp), PACK(size,0));
	}

	/* Case 3 */
	else if (!prev_alloc && next_alloc) {
    	size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    	PUT(FTRP(bp), PACK(size, 0));
    	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    	bp = PREV_BLKP(bp);
	}

	/* Case 4 */
	else {
    	size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
    		GET_SIZE(FTRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	
	return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);

    if (newptr == NULL)
      return NULL;

    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);

    if (size < copySize)
      copySize = size;

    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);

    return newptr;
}

