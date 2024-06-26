/******************************
*  Github:yichenyiyan  QAQ    *
*  							  *
*  							  *
*  							  *
*******************************/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PAGE_SIZE  4096
#define MP_ALIGNMENT 16
#define mp_align(n, alignment)   (((n) + (alignment - 1)) & ~(alignment - 1))
#define mp_align_ptr(p, alignment)   (void*)((((size_t)p) + (alignment - 1)) & ~(alignment - 1))


//学习参考了：https://cloud.tencent.com/developer/article/2325061

//每4k一个block结点
struct mp_node_s {
    unsigned char* end;   //块的结尾（地址）
    unsigned char* last;  //使用到哪了
    struct mp_node_s* next; //链表
    int quote;  //引用计数
    int failed; //失效次数 
};


struct mp_large_s {
    struct mp_large_s* next;
    int size;  //alloc的大小
    void* alloc; //大块内存的起始地址
};


struct mp_pool_s {
    struct mp_large_s* large; //大块内存块链表
    struct mp_node_s* head;  //正常内存块一个链表（此案例中小于等于4K）
    struct mp_node_s* current;
};





/// @brief 创建内存池
/// @return 返回内存池实例的首地址
struct mp_pool_s*
    mp_create_pool(size_t size) {
    struct mp_pool_s* pool;
    if (size < PAGE_SIZE || size % PAGE_SIZE != 0)
        size = PAGE_SIZE;
    /*
    *分配4k以上的内存不用malloc, 选择用posix_memalign
    */


    int ret = posix_memalign((void**)&pool, MP_ALIGNMENT, size);//实际上
    if (ret)
        return NULL;

    pool->large = NULL;
    pool->current = pool->head = (unsigned char*)pool + sizeof(struct mp_pool_s);
    pool->head->last = (unsigned char*)pool + sizeof(struct mp_pool_s) + sizeof(struct mp_node_s);
    pool->head->end = (unsigned char*)pool + PAGE_SIZE;
    pool->head->quote = 0;
    pool->head->failed = 0;

    return pool;
}


//销毁内存池
void
mp_destroy_pool(struct mp_pool_s* pool) {
    struct mp_large_s* large;
    for (large = pool->large; large; large = large->next) {
        if (large->alloc)
            free(large->alloc);
    }

    struct mp_node_s* cur, * next;
    cur = pool->head->next;

    while (cur) {
        next = cur->next;
        free(cur);
        cur = next;
    }

    free(pool);
}


void* mp_malloc_large(struct mp_pool_s*, size_t);
void* mp_malloc_block(struct mp_pool_s*, size_t);


/// @brief 
/// @param pool 线程池实例
/// @param size 所需内存空间大小
/// @return 内存空间的首地址
void*
mp_malloc(struct mp_pool_s* pool, size_t size) {
    if (size <= 0)
        return NULL;

    if (size > PAGE_SIZE - sizeof(struct mp_node_s))//allocate 'large' memory(size > 4K)
        return mp_malloc_large(pool, size);
    else {
        //allocate 'small' memory(size <= 4K)
        unsigned char* mem_addr = NULL;
        struct mp_node_s* cur = NULL;
        cur = pool->current;
        while (cur) {
            mem_addr = mp_align_ptr(cur->last, MP_ALIGNMENT);
            if (cur->end - mem_addr >= size) {
                cur->quote++;
                cur->last = mem_addr + size;
                return mem_addr;
            }
            else
                cur = cur->next; //找不到就继续遍历
        }
        return mp_malloc_block(pool, size);//open new space
    }
}

//分配大于4K的内存
void*
mp_malloc_large(struct mp_pool_s* pool, size_t size) {
    unsigned char* big_addr;
    int ret = posix_memalign((void**)&big_addr, MP_ALIGNMENT, size);
    if (ret)
        return NULL;

    struct mp_large_s* large;
    int n = 0;
    for (large = pool->large; large; large = large->next) {
        if (large->alloc == NULL) {
            large->size = size;
            large->alloc = big_addr;
            return big_addr;
        }
        if (n++ > 3)
            break;
    }
    large = mp_malloc(pool, sizeof(struct mp_large_s));
    if (large == NULL) {
        free(big_addr);
        return NULL;
    }

    large->size = size;
    large->alloc = big_addr;
    large->next = pool->large;
    pool->large = large;

    return big_addr;
}


//分配4k的内存
void*
mp_malloc_block(struct mp_pool_s* pool, size_t size) {
    unsigned char* block;
    int ret = posix_memalign((void**)&block, MP_ALIGNMENT, PAGE_SIZE);
    if (ret)
        return NULL;

    struct mp_node_s* new_node = (struct mp_node_s*)block;
    new_node->end = block + PAGE_SIZE;
    new_node->next = NULL;

    unsigned char* ret_addr = mp_align_ptr(block + sizeof(struct mp_node_s), MP_ALIGNMENT);
    new_node->last = ret_addr + size;
    new_node->quote++;

    struct mp_node_s* current = pool->current;
    struct mp_node_s* cur = NULL;

    for (cur = current; cur->next; cur = cur->next) {
        if (cur->failed++ > 4)
            current = cur->next;
    }

    cur->next = new_node;
    pool->current = current;
    return ret_addr;
}

//类似于alloc的作用
void*
mp_calloc(struct mp_pool_s* pool, size_t size) {
    void* mem_addr = mp_malloc(pool, size);
    if (mem_addr) {
        memset(mem_addr, 0, size);
    }
    return mem_addr;
}


//大块的使用free()释放已申请的内存空间
//小块的对其进行标记，回收到内存池，以便于下次使用
void
mp_free(struct mp_pool_s* pool, void* p) {
    //大块
    struct mp_large_s* large;
    for (large = pool->large; large; large = large->next) {
        if (p == large->alloc) {
            free(large->alloc);
            large->size = 0;
            large->alloc = NULL;
            return;
        }
    }

    //小块
    struct mp_node_s* cur = NULL;
    for (cur = pool->head; cur; cur = cur->next) {
        if ((unsigned char*)cur <= (unsigned char*)p && (unsigned char*)p <= (unsigned char*)cur->end) {
            cur->quote--;
            if (cur->quote == 0) {
                if (cur == pool->head)
                    pool->head->last = (unsigned char*)pool + sizeof(struct mp_pool_s) + sizeof(struct mp_node_s);
                else
                    cur->last = (unsigned char*)cur + sizeof(struct mp_node_s);

                cur->failed = 0;
                pool->current = pool->head;
            }
            return;
        }
    }
}


//将block的last置为初始状态，销毁所有大块内存
void
mp_reset_pool(struct mp_pool_s* pool) {
    struct mp_node_s* cur;
    struct mp_large_s* large;

    for (large = pool->large; large; large = large->next) {
        if (large->alloc)
            free(large->alloc);
    }

    pool->large = NULL;
    pool->current = pool->head;
    for (cur = pool->head; cur; cur = cur->next) {
        cur->last = (unsigned char*)cur + sizeof(struct mp_node_s);
        cur->failed = 0;
        cur->quote = 0;
    }

    return;
}



//用于监听状态的函数
void
monitor_mp_poll(struct mp_pool_s* pool, char* tk) {
    printf("\r\n\r\n------start monitor poll------%s\r\n\r\n", tk);
    struct mp_node_s* head = NULL;
    int i = 0;
    for (head = pool->head; head; head = head->next) {
        ++i;
        if (pool->current == head)
            printf("current==>第 %d 块\n", i);

        if (i == 1) {
            printf("第%02d块small block  已使用:%4ld  剩余空间:%4ld  引用:%4d  failed:%4d\n", i,
                (unsigned char*)head->last - (unsigned char*)pool,
                head->end - head->last, head->quote, head->failed);
        }
        else {
            printf("第%02d块small block  已使用:%4ld  剩余空间:%4ld  引用:%4d  failed:%4d\n", i,
                (unsigned char*)head->last - (unsigned char*)head,
                head->end - head->last, head->quote, head->failed);
        }
    }

    struct mp_large_s* large;
    i = 0;
    for (large = pool->large; large; large = large->next) {
        i++;
        if (large->alloc != NULL) {
            printf("第%d块large block  size=%d\n", i, large->size);
        }
    }
    printf("\r\n\r\n------stop monitor poll------\r\n\r\n");

}

int
main1314()
{
    struct mp_pool_s* p = mp_create_pool(PAGE_SIZE);
    monitor_mp_poll(p, "create memory pool");

    void* mp[30];
    int i;
    for (i = 0; i < 30; i++) {
        mp[i] = mp_malloc(p, 512);
    }
    monitor_mp_poll(p, "申请512字节30个");

    for (i = 0; i < 30; i++) {
        mp_free(p, mp[i]);
    }
    monitor_mp_poll(p, "销毁512字节30个");

    int j;
    for (i = 0; i < 50; i++) {
        char* pp = mp_calloc(p, 32);
        for (j = 0; j < 32; j++) {
            if (pp[j]) {
                printf("calloc wrong\n");
                exit(-1);
            }
        }
    }
    monitor_mp_poll(p, "申请32字节50个");

    for (i = 0; i < 50; i++) {
        char* pp = mp_malloc(p, 3);
    }
    monitor_mp_poll(p, "申请3字节50个");


    void* pp[10];
    for (i = 0; i < 10; i++) {
        pp[i] = mp_malloc(p, 5120);
    }
    monitor_mp_poll(p, "申请大内存5120字节10个");

    for (i = 0; i < 10; i++) {
        mp_free(p, pp[i]);
    }
    monitor_mp_poll(p, "销毁大内存5120字节10个");

    mp_reset_pool(p);
    monitor_mp_poll(p, "reset pool");

    for (i = 0; i < 100; i++) {
        void* s = mp_malloc(p, 256);
    }
    monitor_mp_poll(p, "申请256字节100个");

    mp_destroy_pool(p);
    return 0;
}