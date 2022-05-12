/*
 * memlib.c - a module that simulates the memory system.  Needed because it 
 *            allows us to interleave calls from the student's malloc package 
 *            with the system's malloc package in libc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "memlib.h"
#include "config.h"

/* private variables */
static char *mem_start_brk;  /* 힙의 맨 앞 byte의 주소 */
static char *mem_brk;        /* 사용 중인 힙의 마지막 byte 주소 + 1 */
static char *mem_max_addr;   /* 힙의 맨 끝 byte의 주소 */ 

void mem_init(void)
{
    /* mem_start_brk에는 최대 힙 사이즈 만큼 확보한 공간의 header 주소를 저장 */
    if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL) {
	fprintf(stderr, "mem_init_vm: malloc error\n");
	exit(1);
    }

    mem_max_addr = mem_start_brk + MAX_HEAP;  /* 힙의 맨 끝 byte의 주소 */
    mem_brk = mem_start_brk;                  /* 힙의 맨 앞 byte의 주소. 이후에 header + 1 위치로 옮겨주겠지? */
}

/* 
 * mem_deinit - free the storage used by the memory system model
 */
void mem_deinit(void)
{
    free(mem_start_brk);
}

/*
 * mem_reset_brk - reset the simulated brk pointer to make an empty heap
 */
void mem_reset_brk()
{
    mem_brk = mem_start_brk;
}

/*  */
void *mem_sbrk(int incr) 
{
    // 최초의 mem_brk 값은 힙의 맨 앞 byte의 주소
    char *old_brk = mem_brk; 

    if ( (incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
	errno = ENOMEM;
	fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");
	return (void *)-1;
    }

    // mm_init 함수에서 inplicit: 4*WSIZE, explicit: 6*WSIZE가 매개변수로 들어온다
    // inplicit의 경우, 미사용 패딩 | header | footer | prologue header 4칸 할당
    // mem_brk의 위치는 prologue header 주소 + 1
    mem_brk += incr; 
    return (void *)old_brk; // 최초 return 되는 값은 힙의 맨 앞 byte의 주소
}

/*
 * mem_heap_lo - return address of the first heap byte
 */
void *mem_heap_lo()
{
    return (void *)mem_start_brk;
}

/* 
 * mem_heap_hi - return address of last heap byte
 */
void *mem_heap_hi()
{
    return (void *)(mem_brk - 1);
}

/*
 * mem_heapsize() - returns the heap size in bytes
 */
size_t mem_heapsize() 
{
    return (size_t)(mem_brk - mem_start_brk);
}

/*
 * mem_pagesize() - returns the page size of the system
 */
size_t mem_pagesize()
{
    return (size_t)getpagesize();
}
