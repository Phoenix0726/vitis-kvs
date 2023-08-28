#include "config.h"
#include <cstdio>


hItem* hTable;

int hash1(char* key, int ksize) {   // 求hash条目位置
    ull sum = 0;
    for (int i = 0; i < ksize && key[i] != '\0'; i++) {
        sum = sum * 113 + key[i];
    }
    return sum % BucketNum;
}

ull hash2(char* key, int ksize) {   // 求key对应的mark
    ull sum = 0;
    for (int i = 0; i < ksize && key[i] != '\0'; i++)
        sum = sum * 117 + key[i];
    return sum;
}

int find(char* key, int ksize) {
    int i = hash1(key, ksize);
    int p = hTable[i].next;
    while (p != -1) {
        if (hash2(key, ksize) == hTable[p].mk.mark){
            return hTable[p].kv_addr;
        }
        p = hTable[p].next;
    }
    return -1;
}

void strcpy(char* a, char* b, int size) {
    for (int i = 0; i < size; i++)
        b[i] = a[i];
}

void insert_kv(int ksize, int vsize, char* key, char* value, Heap* heap) {
    int size = 2 * sizeof(int) + ksize + vsize;
    heap->kvp -= size;
    *(int*)(heap->heap + heap->kvp + 0) = ksize;
    *(int*)(heap->heap + heap->kvp + sizeof(int)) = vsize;
    strcpy(key, (char*)(heap->heap + heap->kvp + 2 * sizeof(int)), ksize);
    strcpy(value, (char*)(heap->heap + heap->kvp + 2 * sizeof(int) + ksize), vsize);
}

void insert_h(int ksize, int vsize, char* key, char* value, Heap* heap) {
    int h = hash1(key, ksize);
    int hidx = heap->hp / sizeof(hItem);
    hTable[hidx].tag = true;
    hTable[hidx].mk.mark = hash2(key, ksize);
    hTable[hidx].kv_addr = heap->kvp;

    hTable[hidx].next = hTable[h].next;
    hTable[h].next = hidx;
    
    heap->hp += sizeof(hItem);
}

void insert(int ksize, int vsize, char* key, char* value, Heap* heap) {
    if (find(key, ksize) != -1) {
        printf("The key already exist\n");
        return;
    }
    // 插入kv对
    insert_kv(ksize, vsize, key, value, heap);
    // 插入表条目
    insert_h(ksize, vsize, key, value, heap);
}


extern "C" {
void krnl_kvs(char* reqs, char* res, int batchsize, Heap* heap) {
    hTable = (hItem*)(heap->heap);
    for (int i = 0; i < batchsize; i++) {
        // 解析请求
        char op = *reqs;
        int ksize = *(int*)(reqs + 1);
        int vsize = *(int*)(reqs + 5);
        char* key = reqs + 9;
        char* value = reqs + 9 + ksize;
        reqs += 9 + ksize + vsize;

        if (op == 'I') {
            insert(ksize, vsize, key, value, heap);
        }
        else if (op == 'S') {
            int kv_addr = find(key, ksize);
            if (kv_addr == -1) {
                printf("Search result: Not find\n");
                *(int*)res = 0;
                res += sizeof(int);
                continue;
            }
            *(int*)res = *(int*)(heap->heap + kv_addr + sizeof(int));
            strcpy(heap->heap + kv_addr + 2 * sizeof(int) + ksize, res + sizeof(int), *(int*)res);
            printf("Search result: %s\n", heap->heap + kv_addr + 2 * sizeof(int) + ksize);
            res += sizeof(int) + *(int*)res;
        }
    }
}
}