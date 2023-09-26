#include "config.h"
#include <cstdio>


// hItem* hTable;
static int BucketNum;

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

bool equal(char* a, char* b, int size) {
    for (int i = 0; i < size; i++)
        if (a[i] != b[i])
            return false;
    return true;
}

int find(char* key, int ksize, char* heap) {
    hItem* hTable = (hItem*)(heap + 2 * sizeof(int));

    int i = hash1(key, ksize);
    int p = hTable[i].next;
    while (p != -1) {
        if (hTable[p].tag && hash2(key, ksize) == hTable[p].mk.mark || !hTable[p].tag && equal(key, hTable[p].mk.key, MarkSize))
            return hTable[p].kv_addr;
        p = hTable[p].next;
    }
    return -1;
}

void strcpy(char* a, char* b, int size) {
    for (int i = 0; i < size; i++)
        b[i] = a[i];
}

void insert_kv(int ksize, int vsize, char* key, char* value, char* heap) {
    int size = 2 * sizeof(int) + ksize + vsize;
    *(int*)(heap + sizeof(int)) -= size;        // kvp -= size
    *(int*)(heap + *(int*)(heap + sizeof(int)) + 0) = ksize;        // heap + kvp
    *(int*)(heap + *(int*)(heap + sizeof(int)) + sizeof(int)) = vsize;
    strcpy(key, (char*)(heap + *(int*)(heap + sizeof(int)) + 2 * sizeof(int)), ksize);
    strcpy(value, (char*)(heap + *(int*)(heap + sizeof(int)) + 2 * sizeof(int) + ksize), vsize);
}


int h;

void insert_h(int ksize, int vsize, char* key, char* value, char* heap) {
    hItem* hTable = (hItem*)(heap + 2 * sizeof(int));

    int h = hash1(key, ksize);      //** 8s **//
    int hidx = *(int*)heap / sizeof(hItem);     // hp / sizeof(hItem)
    if (ksize > MarkSize) {     // 存标记
        hTable[hidx].tag = true;
        hTable[hidx].mk.mark = hash2(key, ksize);    //** 8s **//
    }
    else {  // 存key本身
        hTable[hidx].tag = false;
        strcpy(key, hTable[hidx].mk.key, ksize);
    }
    hTable[hidx].kv_addr = *(int*)(heap + sizeof(int));     // heap.kvp

    hTable[hidx].next = hTable[h].next;
    hTable[h].next = hidx;
    
    *(int*)heap += sizeof(hItem);
}

void insert(int ksize, int vsize, char* key, char* value, char* heap) {
    // if (find(key, ksize, heap) != -1) {
    //     printf("The key already exist\n");
    //     return;
    // }
    // 插入kv对
    insert_kv(ksize, vsize, key, value, heap);
    // 插入表条目
    insert_h(ksize, vsize, key, value, heap);
}


extern "C" {
void krnl_kvs(char* reqs, char* res, char* heap, int batchSize, int bucketNum) {
    // hTable = (hItem*)(heap + 2 * sizeof(int));
    BucketNum = bucketNum;
    for (int i = 0; i < batchSize; i++) {
        // printf("handle request %d\n", i);
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
        else if (op == 'R') {
            int kv_addr = find(key, ksize, heap);
            if (kv_addr == -1) {
                // printf("Search result: Not find\n");
                *(int*)res = 0;
                res += sizeof(int);
                continue;
            }
            *(int*)res = *(int*)(heap + kv_addr + sizeof(int));
            strcpy(heap + kv_addr + 2 * sizeof(int) + ksize, res + sizeof(int), *(int*)res);
            // printf("Search result: %s\n", heap + kv_addr + 2 * sizeof(int) + ksize);
            res += sizeof(int) + *(int*)res;
        }
    }
}
}