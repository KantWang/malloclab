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

/**********************************
* explicit, segregated free list  *
* header | prev | next | footer   *
**********************************/

#define GET_P(p)        ((char*)*(unsigned int*)(p))
#define PUT_P(p, val)   (*(unsigned int*)(p) = (int)(val))

// bp == heap_listp : prologue header의 주소 + 1에 위치, 즉 prev 주소
#define PREVRP(bp)      ((char*)(bp)) // 현재 블록의 prev 주소
#define NEXTRP(bp)      ((char*)(bp) + WSIZE) // 현재 블록의 next 주소

#define PREV_FREE_BLKP(bp)  (GET_P((char *)(bp))) // prev가 가리키는 free 블록의 header 주소
#define NEXT_FREE_BLKP(bp)  (GET_P((char*)(bp) + WSIZE)) // next가 가리키는 free 블록의 header 주소

#define CHANGE_PREV(bp,val) (PUT_P(PREVRP(bp), val)); // 현재 블록의 prev를 val로 변경
#define CHANGE_NEXT(bp,val) (PUT_P(NEXTRP(bp), val)); // 현재 블록의 next를 val로 변경

/* The only global variable is a pointer to the first block */
static void* heap_listp;
static void* extend_heap(size_t words);
static void* coalesce(void* bp);
static void* find_fit(size_t adjust_size);
static void place(void* bp, size_t adjust_size);
void *mm_malloc(size_t size);
int mm_init(void);

// explit
static void cut_link(void* bp);
static void push_first(void* bp);

/* bp는 현재 블록의 header + 1을 가리킨다 */
/* heap_listp는 prologue header + 1을 가리킨다 */

int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void*)-1) // mem_sbrk는 mem_brk를 return, 최초의 mem_brk는 힙의 맨 앞 byte의 주소
        return -1;
    
    /* 6워드 size로 잘 초기화 된 경우 */
    PUT(heap_listp, 0); // padding
    PUT(heap_listp + (1 * WSIZE), PACK(2 * DSIZE, 1)); // prologue header
    PUT_P(heap_listp + (2 * WSIZE), NULL); // predecessor
    PUT_P(heap_listp + (3 * WSIZE), NULL); // successor
    PUT(heap_listp + (4 * WSIZE), PACK(2 * DSIZE, 1)); // prologue footer
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1)); // epilogue header
    heap_listp += (2 * WSIZE); // head_listp는 implicit때와 동일하게 prologue header의 주소 + 1을 가리킨다

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
    
    // bp == 새로운 블록의 header + 1
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
        push_first(bp); // 연결리스트의 first free 블록을 bp로 변경
    }
    // case2. 다음 블록이 free인 경우
    else if (prev_alloc && !next_alloc) {
        cut_link(NEXT_BLKP(bp)); // 다음 free 블록을 연결 리스트에서 제거
        
        // 현재, 다음 free 블록 연결 (size, header, footer 정보 변경)
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); 
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        push_first(bp); // 병합 마쳤으니 연결리스트의 first free 블록을 bp로 변경
    }
    // case3. 이전 블록이 free인 경우
    else if (!prev_alloc && next_alloc) {
        cut_link(PREV_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        // 현재 블록의 footer에 size 업데이트, 앞 블록의 header에 size 업데이트
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        
        push_first(PREV_BLKP(bp));
        bp = PREV_BLKP(bp); // bp를 앞 블록의 prev로 변경
    }
    // case4. 양쪽 모두 free인 경우
    else {
        // 양쪽 free 블록 모두 연결리스트에서 제거
        cut_link(NEXT_BLKP(bp));
        cut_link(PREV_BLKP(bp));

        size += (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));

        push_first(PREV_BLKP(bp));
        bp = PREV_BLKP(bp); // bp를 앞 블록의 prev로 변경
    }

    return bp; // 최종 free 블록의 prev 리턴
}

/* first fit 방식으로 검색 */
static void *find_fit(size_t adjust_size){ 
    void *bp;

    // 연결리스트의 첫번째 prev에서 시작, 연결리스트의 마지막 free 블록까지 확인
    for (bp = PREV_FREE_BLKP(heap_listp); bp != (char*)NULL; bp = PREV_FREE_BLKP(bp)) 
        // free 블록의 사이즈가 충분할 경우
        if (GET_SIZE(HDRP(bp)) >= adjust_size) 
            return bp; // bp에 할당해 줘!
        
    // 검색에 실패할 경우
    return NULL; // 이 경우 extend_heap
}

// malloc(sizeof(int) * size); 요청이 있을 경우, header | pred | next | footer + size 만큼 할당 가능한 free 블록을 찾는다
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
        cut_link(bp); // free였던 블록이 잘 할당 되었으니 연결리스트에서 제거
        PUT(HDRP(bp), PACK(cur_size, 1));
        PUT(FTRP(bp), PACK(cur_size, 1));
     }
     // 남은 공간을 잘라서 새로운 free 블록 생성
     else{
        // 우선 adjust_size 만큼 할당해주고
        PUT(HDRP(bp), PACK(adjust_size, 1));
        PUT(FTRP(bp), PACK(adjust_size, 1));
        // 연결리스트에서 bp 제거한 후
        cut_link(bp);
        // 남는 블록을 새로운 free 블록으로 
        PUT(HDRP(NEXT_BLKP(bp)), PACK(cur_size - adjust_size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(cur_size - adjust_size, 0));
        // 새로운 free 블록을 연결리스트의 first로 추가
        push_first(NEXT_BLKP(bp));
     }
}

static void cut_link(void* bp) {
    if (PREV_FREE_BLKP(bp) != (char*)NULL) 
        CHANGE_NEXT(PREV_FREE_BLKP(bp), NEXT_FREE_BLKP(bp)); // bp의 prev가 가리키던 free 블록의 next를 bp의 next로 변경
    
    if (NEXT_FREE_BLKP(bp) != (char*)NULL) 
        CHANGE_PREV(NEXT_FREE_BLKP(bp), PREV_FREE_BLKP(bp)); // bp의 next가 가리키던 free 블록의 prev를 bp의 prev로 변경
    
}

static void push_first(void* bp) {
    // 원래 free 블록이 존재했을 경우
    if (PREV_FREE_BLKP(heap_listp) != (char*)NULL)
        // 원래 first free 블록의 next를 bp로 변경
        CHANGE_NEXT(PREV_FREE_BLKP(heap_listp), bp);
    
    PUT_P(PREVRP(bp), PREV_FREE_BLKP(heap_listp)); // 새로운 free 블록의 prev에 직전 first free 블록의 prev 주소 저장
    PUT_P(NEXTRP(bp), heap_listp); // 새로운 free 블록의 next에 prologue prev 주소 저장
    PUT_P(PREVRP(heap_listp), bp); // prologue prev에 bp 저장
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