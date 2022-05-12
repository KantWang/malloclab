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
    "Team8",
    /* First member's full name */
    "KantWang",
    /* First member's email address */
    "dngp93@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* Basic constants and macros */
#define WSIZE 4              /* Word and header/footer size (bytes) */
#define DSIZE 8              /* Double word size (bytes) */
#define CHUNKSIZE (1<<12)    /* Extend heap by this amount (bytes) */

#define MAX(x, y)       ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(unsigned int *)(p))
#define PUT(p, val)     (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* The only global variable is a pointer to the first block */
static void* heap_listp;
static void* extend_heap(size_t words);
static void* coalesce(void* bp);
static void* find_fit(size_t adjust_size);
static void place(void* bp, size_t adjust_size);
void *mm_malloc(size_t size);
int mm_init(void);

/* bp는 현재 블록의 header + 1을 가리킨다 */
/* heap_listp는 prologue header + 1을 가리킨다 */

int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1){ // mem_sbrk는 mem_brk를 return, 최초의 mem_brk는 힙의 맨 앞 byte의 주소
        return -1;
    }
    /* 4워드 size로 잘 초기화 된 경우 */
    PUT(heap_listp, 0); // padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // prologue block header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // prologue block footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); // epilogue block header
    heap_listp += (2 * WSIZE); // prologue header + 1로 변경
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) // CHUNKSIZE / WSIZE 칸 만큼 힙을 늘린다
        return -1; 

    return 0;
}

/*
    아래 두가지 상황에 extend_heap 호출
    1. 힙 초기화 시 호출
    2. mm_malloc의 find_fit이 실패했을 때
*/
static void* extend_heap(size_t words) // 힙을 words * WSIZE byte만큼 확장. malloc 인자로 CHUNKSIZE보다 작은 값이 들어올 경우 CHUNKSIZE만큼 늘려달라는 요청으로 보정되어 들어온다
{
    char* bp;
    size_t size;

    // 정렬을 유지하기 위해서 요청 받은 크기를 인접 2워드의 배수(8byte의 배수)로 반올림
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; 
    
    // bp에는 기존 힙의 우측 끝 주소 값이 저장됨
    if ((long)(bp = mem_sbrk(size)) == -1) 
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0)); // header에 PACK(size, 0) 저장
    PUT(FTRP(bp), PACK(size, 0)); // footer에 PACK(size, 0) 저장
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // epilogue header에 PACK(0, 1) 저장
    return coalesce(bp); // 할당 전 마지막 블록이 free라면 새로 할당한 블록과 coalesce
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

// 앞, 현재, 뒷 블록 확인하고 병합
static void *coalesce(void* bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // bp의 이전 블록
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // bp의 다음 블록
    size_t size = GET_SIZE(HDRP(bp)); // 현재 블록의 사이즈
    
    // case1. 양쪽 모두 할당된 경우
    if (prev_alloc && next_alloc) {
        return bp;
    }
    // case2. 다음 블록이 free인 경우
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // free 블록 사이즈 += 뒷 블록의 사이즈
        // 현재 블록의 header에 size 업데이트, 뒷 블록의 footer에 size 업데이트
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // case3. 이전 블록이 free인 경우
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        // 현재 블록의 footer에 size 업데이트, 앞 블록의 header에 size 업데이트
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp); // bp를 앞 블록의 header+1로 이동
    }
    // case4. 양쪽 모두 free인 경우
    else {
        size += (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp); // bp를 앞 블록의 header+1로 이동
    }
    return bp; // 최종 free 블록의 header+1 리턴
}

/* first fit 방식으로 검색 */
static void *find_fit(size_t adjust_size){
    void *bp;
    
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){ 
        // prologue header + 1에서 epilogue header까지
        if (!GET_ALLOC(HDRP(bp)) && (adjust_size<=GET_SIZE(HDRP(bp)))){ // 가용 블록이고 size가 충분하다면
            return bp; // bp에 할당해 줘!
        }
    }
    // 검색에 실패할 경우
    return NULL; // 이 경우 extend_heap
}

void *mm_malloc(size_t size)
{
    size_t adjust_size; // size 그대로 place 할 수 없다, 조정 필요
    size_t extend_size; // 적절한 free 블록을 찾지 못했을 때, extend_size 만큼 힙 확장 
    char* bp;

    // 잘못된 할당 요청은 무시
    if (size == 0)
        return NULL;

    // 2words 이하의 사이즈는 4워드로 할당 요청 (header 1word, footer 1word)
    if (size <= DSIZE)
        adjust_size = DSIZE * 2; 
    // 할당 요청의 용량이 2words 초과 시, 충분한 8byte의 배수의 용량 할당
    else 
        adjust_size = DSIZE * ((size+(DSIZE)+(DSIZE - 1)) / DSIZE);

    // 사이즈에 맞는 위치 탐색하여 할당
    if ((bp = find_fit(adjust_size)) != NULL) // first fit 방식으로 찾은 블록의 주소
    {
        place(bp, adjust_size);
        return bp;
    }
    // 사이즈에 맞는 위치가 없는 경우, 추가적으로 힙 영역 요청 및 배치 후 할당
    extend_size = MAX(adjust_size, CHUNKSIZE);
    if ((bp = extend_heap(extend_size / WSIZE)) == NULL)
        return NULL;

    place(bp, adjust_size);
    return bp;
}

static void place(void* bp, size_t adjust_size){
    size_t cur_size = GET_SIZE(HDRP(bp));

     if (cur_size - adjust_size < 2*DSIZE){ // 메모리를 할당하고 남은 공간이 16byte 미만일 때는 따로 뒤에 헤더, 푸터를 만들어주지 않는다. 패딩처리
         PUT(HDRP(bp), PACK(cur_size, 1));
         PUT(FTRP(bp), PACK(cur_size, 1));
     }
     // 남은 공간을 잘라서 새로운 free 블록 생성
     else{
         // 우선 adjust_size 만큼 할당해주고
         PUT(HDRP(bp), PACK(adjust_size, 1));
         PUT(FTRP(bp), PACK(adjust_size, 1));
         // 남는 블록을 새로운 free 블록으로 
         PUT(HDRP(NEXT_BLKP(bp)), PACK(cur_size - adjust_size, 0));
         PUT(FTRP(NEXT_BLKP(bp)), PACK(cur_size - adjust_size, 0));
     }
}

/*
   기존에 malloc으로 동적 할당된 메모리 크기를 변경시켜주는 함수
   현재 메모리에 bp가 가르키는 사이즈를 할당한 만큼 충분하지 않다면 메모리의 다른 공간의 기존 크기의 공간 할당 + 기존에 있던 데이터를 복사한 후 추가로 메모리 할당
*/
void* mm_realloc(void* bp, size_t size) 								
{
	void* old_dp = bp;
	void* new_dp;
	size_t copySize;

	new_dp = mm_malloc(size); // 변경할 사이즈 만큼 할당을 받고											  
	if (new_dp == NULL)													  
		return NULL;

	copySize = GET_SIZE(HDRP(old_dp)); // 기존 사이즈를 copySize에 저장		  
	if (size < copySize) // 데이터 손실이 발생하는 case   
		copySize = size; 
	memcpy(new_dp, old_dp, copySize); // 새로 할당 받은 곳에 원래 데이터를 copySize만큼 복사
	mm_free(old_dp); // 기존 메모리는 free
	return new_dp; // 새로운 주소 return
}











