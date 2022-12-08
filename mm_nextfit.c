/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE        4        /* Word and header / footer size (bytes) */
#define DSIZE        8        /* Double word size (bytes) */
#define CHUNKSIZE    (1<<12)  /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word*/
#define PACK(size, alloc)    ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)            (*(unsigned int *)(p))
#define PUT(p, val)       (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)    (GET(p) & ~0x7)
#define GET_ALLOC(p)   (GET(p) & 0x1)

/* Given block prt bp, compute address of its header and footer */	
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)    ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)    ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))



static char *heap_listp;
static char *last_alloc_bp;



/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                             /* Alignment padding */ 
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        /* Epilogue header */
    heap_listp += (2*WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    // last_alloc_bp를 초기화 해준다. 
    last_alloc_bp = heap_listp;
    return 0; 
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allcoate and even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    // free block을 할당 받고 헤더, 푸터에 allocate bit를 0으로 준다.
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));    // 새로운 에필로그 헤더

    /* Coalesce if previous block was free */
    return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;	// Adjusted block size, 8의 배수형태로 사이즈를 지정해줘야 한다.
    size_t extendsize;  // Amount to extend heap if no fit
    char *bp;

    // Ignore spurious requests
    if (size == 0)
        return NULL;

    // Adjust block size to include overhead and alignment requests
    if (size <= DSIZE)
        asize = 2*DSIZE;                                               // 할당하고자 하는 사이즈가 8의 배수보다 작다면, 16바이트를 할당한다.
    else
	asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);        // 2 words 정렬을 만족하기 위해 8의 배수 만큼 사이즈를 할당.

    // Search the free list for a fit
    // 할당 사이즈가 들어갈 빈 공간을 찾았다면 그 자리에 할당해줌.
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
	last_alloc_bp = bp;
        return bp;
    }	
    
    // No fit found. Get more memory and place the block
    // 맞는 상이즈가 없다면 추가 메모리를 할당 해주도록 요청.
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    last_alloc_bp = bp;
    return bp;

}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp)); 
    // allocate bit 를 0으로 만들어줘서 다음 malloc에서 해당 블럭을 덮어쓸 수 있도록 한다. 
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * 할당 해제하고 나서 사용 가능해진 블록둘울 합쳐준다.
 * */
static void *coalesce(void *bp)
{
    // 이전 블럭의 allocate bit를 가져온다.
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));

    // 다음 블럭의 allocate bit를 가져온다.
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    // 현재 블럭의 사이즈를 가져온다.
    size_t size = GET_SIZE(HDRP(bp));

    // Case 1 이전, 다음 블럭의 allocate bit가 1일 때, 합칠 수 없다.
    if (prev_alloc && next_alloc)
    {
	last_alloc_bp = bp;
        return bp;
    }
    // Case 2 다음 블럭만 사용가능 할 때, 다음 블럭을 합치고 allocate bit 를 0으로 만들어준다. 
    else if (prev_alloc && !next_alloc)
    {
	// 다음 블럭의 크기를 가져와서 현재 블럭에 더해준다.
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // Case 3 이전 블럭만 사용 가능할 때, 이전 블럭을 합치고 allocate bit를 0으로 만들어 준다.
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	// 블럭 포인터를 이전 블럭의 것으로 바꿔줘야 한다.
	bp = PREV_BLKP(bp);
    }

    // Case 4 양쪽 블럭 다 사용가능 할 경우 
    else
    {
        size += GET_SIZE(FTRP(NEXT_BLKP(bp))) +
	       	GET_SIZE(HDRP(PREV_BLKP(bp)));
	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);	
    }
    
    last_alloc_bp = bp;
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
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}


/* First-fit search */
/*static void *find_fit(size_t asize)
{
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
	{
	    return bp;
	}
    }
    // No fit
    return NULL; 
}
*/

static void *find_fit(size_t asize)
{
    // 마지막으로 할당된 부분부터 탐색 시작.
    char *bp = last_alloc_bp;

    for (bp = NEXT_BLKP(bp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {		
        if (GET_SIZE(HDRP(bp)) >= asize && !GET_ALLOC(HDRP(bp)))
        {
	    // 할당 할 수 있는 공간을 찾으면 last_alloc_bp를 갱신해준다.
	    last_alloc_bp = bp;
            return bp;
        }
    }

    // 가용공간이 없다면 heap_listp부터 다시찾는다.
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
	{
	    last_alloc_bp = bp;
	    return bp;
	}
    }
    // No fit
    return NULL;
}


static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
	bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}











