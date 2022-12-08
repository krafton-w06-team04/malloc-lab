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
#include <mm_malloc.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Super Low Man's club",
    /* First member's full name */
    "Cho Jehee",
    /* First member's email address */
    "cho5048@dgu.ac.kr",
    /* Second member's full name (leave blank if none) */
    "Park Juhwan",
    /* Second member's email address (leave blank if none) */
    "jhpark4947@gmail.com",
    /* Third member's full name (leave blank if none) */
    "Ahn Juhong",
    /* Third member's email address (leave blank if none) */
    "juhong.ahn.dev@gmail.com",
};

/* single word (4) or double word (8) alignment */
#define WSIZE 4             // single word size
#define DSIZE 8             // double word size
#define CHUNKSIZE (1 << 12) // extend heap by this amount when needed

#define max(a, b) ((a) > (b) ? (a) : (b))

// Pack a size and allocated bit into a word
#define PACK(size, alloc) ((size) | (alloc))

// Read and write a word at address p
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// Read the size and allocated fields from address p
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char *)(bp)-WSIZE)                        // header의 주소
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // footer의 주소

// Given block ptr bp, compute address of next and previous blocks
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (DSIZE - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // 8

static char *heap_listp;
char *next_search;

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // Create the initial empty heap
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                            // Alignment padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // Prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // Prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // Epilogue header
    heap_listp += (2 * WSIZE);
    next_search = heap_listp;
    // extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // allocate an even number of words to maintain alignment
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // words가 홀수면 +1을 해주고 곱해줌
    if ((long)(bp = mem_sbrk(size)) == -1)                    // TODO: bp에 무슨 값이 들어오는지 확인하기
        return NULL;

    // Initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(size, 0));         // Free block header
    PUT(FTRP(bp), PACK(size, 0));         // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // New epilogue header

    // Coalesce if the previous block was free
    return coalesce(bp);
}

static void *find_fit(size_t asize)
{
    void *bp;

    // First-fit search

    // for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) // 계속 다음 블록 돌면서
    //     if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))    // 넣을 사이즈가 할당되어 있지 않고 충분한 공간이 존재할 때
    //         return bp;                                                // 그곳의 주소를 리턴한다

    // Next-fit search
    // if (next_search)
    // {
    //     for (bp = next_search; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) // 계속 다음 블록 돌면서
    //         if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))     // 넣을 사이즈가 할당되어 있지 않고 충분한 공간이 존재할 때
    //         {
    //             next_search = bp; // 그곳의 주소를 리턴한다
    //             return next_search;
    //         }
    // }
    // else
    // {
    for (bp = next_search; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) // 계속 다음 블록 돌면서
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))     // 넣을 사이즈가 할당되어 있지 않고 충분한 공간이 존재할 때
        {
            next_search = bp; // 그곳의 주소를 리턴한다
            return next_search;
        }
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) // 계속 다음 블록 돌면서
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))    // 넣을 사이즈가 할당되어 있지 않고 충분한 공간이 존재할 때
            return bp;                                                // 그곳의 주소를 리턴한다

    // }

    return NULL; // No fit
}

static void place(void *bp, size_t alloc_size)
{
    size_t cur_size = GET_SIZE(HDRP(bp)); // 현재 bp의 블록 size

    if ((cur_size - alloc_size) >= (2 * DSIZE)) //
    {
        PUT(HDRP(bp), PACK(alloc_size, 1));
        PUT(FTRP(bp), PACK(alloc_size, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(cur_size - alloc_size, 0));
        PUT(FTRP(bp), PACK(cur_size - alloc_size, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(cur_size, 1));
        PUT(FTRP(bp), PACK(cur_size, 1));
    }
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extend_size;
    char *bp;

    // 할당할 사이즈가 0이면 NULL 반환
    if (size == 0)
        return NULL;

    // double word 만큼 사이즈 할당
    asize = ALIGN(size + SIZE_T_SIZE);

    // Search the free list for a fit
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    // No fit found. Get more memory and place the block
    extend_size = max(asize, CHUNKSIZE);
    if ((bp = extend_heap(extend_size / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
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
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // bp 이전 블록이 할당 됐는지
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // bp 다음 블록이 할당 됐는지
    size_t size = GET_SIZE(HDRP(bp));                   // bp의 size 구하기

    // 둘 다 할당되어 있으면 합칠 필요가 없음
    if (prev_alloc && next_alloc)
        return bp;
    // 이전 블록은 할당되어 있고 다음 블록은 할당되어 있지 않을때는
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블록(free) 헤더에서 free 블록의 사이즈를 구함
        PUT(HDRP(bp), PACK(size, 0));          // 원래 bp size에 다음 블록의 size를 합치고
        PUT(FTRP(bp), PACK(size, 0));          // header와 footer에 size와 free 정보를 넣어줌
    }
    // 이전 블록: free / 다음 블록: allocated
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));   // 이전 블록의 정보에서 size 구해서 현 bp size와 더해줌
        PUT(FTRP(bp), PACK(size, 0));            // bp footer에 size, free 정보 넣어줌
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록의 헤더에 size, free 정보 넣어줌
        bp = PREV_BLKP(bp);                      // bp를 이전 블록의 bp를 가리키게 만듦
    }
    // 이전 블록, 다음 블록: free
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 세 블록 다 이어버림
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));                               // 이전 블록의 헤더에 size, free 넣어줌
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));                               // 다음 블록의 footer에 size, free 넣어줌
        bp = PREV_BLKP(bp);                                                    // bp를 이전 블록의 bp로 바꿔줌
    }
    next_search = bp;
    return bp; // 포인터 반환
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t c_size = GET_SIZE(HDRP(oldptr)); // block의 크기
    // size_t a_size = ALIGN(size + SIZE_T_SIZE); // realloc block의 크기

    if (!ptr)
        return mm_malloc(size);

    // if (size >= c_size)
    // {
    //     if (!GET_ALLOC(NEXT_BLKP(oldptr)) && GET_SIZE(NEXT_BLKP(oldptr)) >= size - c_size)
    //     {
    //         size_t next_block_size = GET_SIZE(HDRP(NEXT_BLKP(oldptr))); //
    //         // void *next_block_header = HDRP(NEXT_BLKP(oldptr));
    //         size_t new_size;

    //         // PUT(HDRP(NEXT_BLKP(oldptr)), PACK(0, 0));       // 원래 다음 블록 header 초기화
    //         // PUT(FTRP(oldptr), PACK(0, 0));                  // 원래 oldptr footer 초기화
    //         PUT(HDRP(oldptr), PACK(size, 1));                 // oldptr header에 늘어난 size 정보 입력
    //         newptr = NEXT_BLKP(oldptr);                       // NEXT_BLKP는 oldptr header에서 size 정보 가져오기 때문에 가능
    //         PUT(FTRP(oldptr) + 3 * WSIZE, PACK(size + 8, 1)); // 새로운 oldptr footer에 늘어난 size, alloc 정보 입력
    //         PUT(HDRP(oldptr), PACK(size + 8, 1));
    //         new_size = next_block_size - (size - c_size); // size 조정 후 뒷쪽 가용 블록의 크기
    //         PUT(HDRP(newptr), PACK(new_size, 0));         // 다음 블록 header에 가용 블록 정보 넣어줌
    //         PUT(FTRP(newptr), PACK(new_size, 0));         // 다음 블록 footer에 가용 블록 정보 넣어줌
    //         return oldptr;
    //     }
    // }

    newptr = mm_malloc(size);
    if (!newptr)
        return NULL;
    if (size == 0)
    {
        mm_free(ptr);
        return NULL;
    }
    if (size < c_size) // realloc하려는 size가 ptr에 들어있는 copysize보다 작으면 해준다
        c_size = size;
    memcpy(newptr, oldptr, c_size);
    mm_free(oldptr);
    return newptr;
}
