/*
 * mm-explicit.c - Implementation of malloc using an explicit list.
 * 
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <stdio.h>
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

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */  
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */
// CHUNKSIZE: default size for expanding the heap

/*Max value of 2 values*/
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p)) // pointer
#define PUT(p, val)  (*(unsigned int *)(p) = (val))  

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)  // address
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given block ptr bp, get the address of next and previous free blocks */
#define GET_NEXT_BLKP(bp) (*(char **)(bp + WSIZE))
#define GET_PREV_BLKP(bp) (*(char **)(bp))

/* Given block ptr bp, set the address of next and previous free blocks */
#define SET_NEXT_BLKP(bp, np) (GET_NEXT_BLKP(bp) = np)
#define SET_PREV_BLKP(bp, np) (GET_PREV_BLKP(bp) = np)

/* The two global variables: a pointer to the first block and the pointer to the first free block*/
static char *heap_listp = 0; 
static char *free_startp = 0;

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp); 
static void checkblock(void *bp);

static void put_free(void *bp); 
static void del_free(void *bp); 
static void checkheap(int verbose);

/* 
 * mm_init - Initialize the memory manager 
 */
int mm_init(void) {
  
  /* Create the initial empty heap. */
  if ((heap_listp = mem_sbrk(8*WSIZE)) == NULL) 
    return -1;

  PUT(heap_listp, 0);                        /* Alignment padding */
  PUT(heap_listp+(1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
  PUT(heap_listp+(2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
  PUT(heap_listp+(3*WSIZE), PACK(0, 1));     /* Epilogue header */
  heap_listp += (2*WSIZE);
  free_startp = heap_listp + (2*WSIZE); 

  /* Extend the empty heap with a free block of minimum possible block size */
  if (extend_heap(WSIZE) == NULL){ 
    return -1;
  }
  return 0;
}

/* 
 * mm_malloc - Allocate a block with at least size bytes of payload 
 */
void *mm_malloc(size_t size) 
{
   size_t asize;      /* Adjusted block size */
   size_t extendsize; /* Amount to extend heap if no fit */
   void *bp;

   /* Ignore spurious requests */
   if (size == 0) return (NULL);

   /* Adjust block size to include overhead and alignment reqs */
   if (size <= DSIZE)
	asize = 2*DSIZE;
   else
	asize = DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE);

   /* Search the free list for a fit */
   if ((bp = find_fit(asize)) != NULL) {
	place(bp, asize);
	return (bp);
   }

   /* No fit found.  Get more memory and place the block */
   extendsize = MAX(asize,CHUNKSIZE);
   if ((bp = extend_heap(extendsize/WSIZE)) == NULL)  
	return NULL;
   place(bp, asize);
   return bp;
}

/* 
 * mm_free - Free a block 
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * Requires:
 *   "ptr" is either the address of an allocated block or NULL.
 *
 * Effects:
 *   Reallocates the block "ptr" to a block with at least "size" bytes of
 *   payload, unless "size" is zero.  
 *   If "size" is zero, frees the block "ptr" and returns NULL.  
 *   If the block "ptr" is already a block with at
 *   least "size" bytes of payload, then "ptr" may optionally be returned.
 *   Further if requested size is greater than the current block size at pointer bp
 *   and we have the next block as empty with sum of current block size and next block (which happens to be empty)
 *   then we dont need call malloc but just combine current block and next block to resize them so as to 
 *   satisfy the requested realloc size. 
 *   If nothing can be done then a new block is allocated (using malloc) and the contents of the old block
 *   "ptr" are copied to that new block.  Returns the address of this new
 *   block if the allocation was successful and NULL otherwise.
 */
/*
void *mm_realloc(void *bp, size_t size) {
   if (size == 0) {
	mm_free(bp);
	return NULL;
   }

   size_t old_size = GET_SIZE(HDRP(bp));

}
*/

void *mm_realloc(void *bp, size_t size){
  if((int)size < 0) 
    return NULL; 
  else if((int)size == 0){ 
    mm_free(bp); 
    return NULL; 
  } 
  else if(size > 0){ 
      size_t oldsize = GET_SIZE(HDRP(bp)); 
      size_t newsize = size + 2 * WSIZE; // 2 words for header and footer
      /*if newsize is less than oldsize then we just return bp */
      if(newsize <= oldsize){ 
          return bp; 
      }
      /*if newsize is greater than oldsize */ 
      else { 
          size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); 
          size_t csize;
          /* next block is free and the size of the two blocks is greater than or equal the new size  */ 
          /* then we only need to combine both the blocks  */ 
          if(!next_alloc && ((csize = oldsize + GET_SIZE(  HDRP(NEXT_BLKP(bp))  ))) >= newsize){ 
            del_free(NEXT_BLKP(bp)); 
            PUT(HDRP(bp), PACK(csize, 1)); 
            PUT(FTRP(bp), PACK(csize, 1)); 
            return bp; 
          }
          else {  
            void *new_ptr = mm_malloc(newsize);  
            place(new_ptr, newsize);
            memcpy(new_ptr, bp, newsize); 
            mm_free(bp); 
            return new_ptr; 
          } 
      }
  }else 
    return NULL;
} 

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;
	
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((int)(bp = mem_sbrk(size)) == -1) 
	return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp) 
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));

    if (PREV_BLKP(bp) == bp) {
	prev_alloc = prev_alloc || 1;
    }

    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
	// do nothing
    }

    else if (prev_alloc && !next_alloc) {     
	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
	del_free(NEXT_BLKP(bp));
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {   
	size += GET_SIZE(HDRP(PREV_BLKP(bp)));
	del_free(PREV_BLKP(bp));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	bp = PREV_BLKP(bp);
    }

    else {                                    
	size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
	    GET_SIZE(FTRP(NEXT_BLKP(bp)));
	del_free(PREV_BLKP(bp));
	del_free(NEXT_BLKP(bp));
	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
	bp = PREV_BLKP(bp);
    }
    put_free(bp);
    return bp;
}

/* 
 * find_fit - For a block with asize bytes, find a fit.
 */

static void *find_fit(size_t asize){

    void *bp;
    void *temploc;
    temploc = NULL;
    unsigned int temploc_size=99999999;
    unsigned int curr_size=99999999;
    for (bp = heap_listp; (curr_size = GET_SIZE(HDRP(bp))) > 0; bp = NEXT_BLKP(bp)) {
		if( !GET_ALLOC(HDRP(bp)) && ( asize <= curr_size ) && temploc_size >  curr_size ) {
			temploc = bp;
			temploc_size = curr_size;
		}
    }

    return temploc;
}
/*
static void *find_fit(size_t asize){
  void *bp;
  static int last_malloced_size = 0;
  static int repeat_counter = 0;
  if( last_malloced_size == (int)asize){
      if(repeat_counter>32){  
        int extendsize = MAX(asize, 4 * WSIZE);
        bp = extend_heap(extendsize/4);
        return bp;
      }
      else
        repeat_counter++;
  }
  else
    repeat_counter = 0;
  for (bp = free_startp; GET_ALLOC(HDRP(bp)) == 0; bp = GET_NEXT_BLKP(bp) ){
    if (asize <= (size_t)GET_SIZE(HDRP(bp)) ) {
      last_malloced_size = asize;
      return bp;
    }
  }
  return NULL;
}
*/

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size CHANGE
 */
static void place(void *bp, size_t asize){
   size_t csize = GET_SIZE(HDRP(bp));

   if ((csize - asize) >= (2*DSIZE)) {
	PUT(HDRP(bp), PACK(asize, 1));
	PUT(FTRP(bp), PACK(asize, 1));
	del_free(bp);
	bp = NEXT_BLKP(bp);
	PUT(HDRP(bp), PACK(csize-asize, 0));
	PUT(FTRP(bp), PACK(csize-asize, 0));
	coalesce(bp);
   }
   else {
	PUT(HDRP(bp), PACK(csize, 1));
	PUT(FTRP(bp), PACK(csize, 1));
	del_free(bp);
  }
}

static void put_free(void *bp) {
   SET_NEXT_BLKP(bp, free_startp); 
   SET_PREV_BLKP(free_startp, bp); 
   SET_PREV_BLKP(bp, NULL); 
   free_startp = bp;
}

static void del_free(void *bp){
   if (GET_PREV_BLKP(bp) != NULL) {
	SET_NEXT_BLKP(GET_PREV_BLKP(bp), GET_NEXT_BLKP(bp));
	SET_PREV_BLKP(GET_NEXT_BLKP(bp), GET_PREV_BLKP(bp));
   }
   else {
	free_startp = GET_NEXT_BLKP(bp);
	SET_PREV_BLKP(GET_NEXT_BLKP(bp), GET_PREV_BLKP(bp));
   }
}


/* 
 * The remaining routines are heap consistency checker routines. 
 */

/*
 * Requires:
 *   "bp" is the address of a block.
 *
 * Effects:
 *   Perform a minimal check on the block "bp".
 */
static void
checkblock(void *bp) 
{

  if ((unsigned int)bp % DSIZE)
    printf("Error: %p is not doubleword aligned\n", bp);
  if (GET(HDRP(bp)) != GET(FTRP(bp)))
    printf("Error: header does not match footer\n");
}

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Perform a minimal check of the heap for consistency. 
 */
void
checkheap(int verbose) 
{
  void *bp;

  if (verbose)
    printf("Heap (%p):\n", heap_listp);

  if (GET_SIZE(HDRP(heap_listp)) != DSIZE ||
      !GET_ALLOC(HDRP(heap_listp)))
    printf("Bad prologue header\n");
  checkblock(heap_listp);

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = (void *)NEXT_BLKP(bp)) {
    if (verbose)
      printblock(bp);
    checkblock(bp);
  }

  if (verbose)
    printblock(bp);
  if (GET_SIZE(HDRP(bp)) != 0 || !GET_ALLOC(HDRP(bp)))
    printf("Bad epilogue header\n");
}

/*
 * Requires:
 *   "bp" is the address of a block.
 *
 * Effects:
 *   Print the block "bp".
 */
static void
printblock(void *bp) 
{
  int halloc, falloc;
  size_t hsize, fsize;

  checkheap(0);
  hsize = GET_SIZE(HDRP(bp));
  halloc = GET_ALLOC(HDRP(bp));  
  fsize = GET_SIZE(FTRP(bp));
  falloc = GET_ALLOC(FTRP(bp));  

  if (hsize == 0) {
    printf("%p: end of heap\n", bp);
    return;
  }

  printf("%p: header: [%zu:%c] footer: [%zu:%c]\n", bp, 
      hsize, (halloc ? 'a' : 'f'), 
      fsize, (falloc ? 'a' : 'f'));
}

