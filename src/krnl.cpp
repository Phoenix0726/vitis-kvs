#include <cstdio>

#define BucketNum 13
#define HashItemNum BucketNum * 4
#define ValueNum HashItemNum
#define ValueMaxSize 8


void strcpy(char* a, char* b) {
    if (b == nullptr) {
        a = nullptr;
        return;
    }
    for (int i = 0; i < ValueMaxSize; i++)
        a[i] = b[i];
}

int strlen(char* s) {
    int i = 0;
    while (s[i] != '\0')
        i++;
    return i;
}

typedef char ValItem[ValueMaxSize];

struct hItem {
    int key;
    int v_addr;
    int next;
};
hItem hTable[HashItemNum];  // 哈希表
int hidx = BucketNum;       // 哈希表索引

struct vItem    // value item
{
    int size;
    ValItem val;
};
vItem values[ValueNum];     // values
int vidx = 0;               // 值索引

struct ReqItem {
    char op;
    int key;
    ValItem value;
};

void hash_init() {
    for (int i = 0; i < HashItemNum; i++) {
        hTable[i].v_addr = -1;
        hTable[i].next = -1;
    }
    for (int i = 0; i < ValueNum; i++) {
        values[i].size = -1;
        for (int j = 0; j < ValueMaxSize; j++)
            values[i].val[j] = '\0';
    }
}

int hash(int key) {
    return key % BucketNum;
}

int find(int key) {     // 返回值地址
    int i = hash(key);
    int p = hTable[i].next;
    while (p != -1) {
        if (hTable[p].key == key)
            return hTable[p].v_addr;
        p = hTable[p].next;
    }
    return -1;
}

int insert(int key, char* value) {
    if (find(key) != -1) {
        printf("The key already exist\n");
        return 0;
    }
    int h = hash(key);

    // 插入值条目
    values[vidx].size = strlen(value);
    strcpy(values[vidx].val, value);

    // 插入表条目
    hTable[hidx].key = key;
    // fence
    hTable[hidx].v_addr = vidx;

    hTable[hidx].next = hTable[h].next;
    hTable[h].next = hidx;
    hidx++; vidx++;
    return 1;
}


extern "C" {
void krnl_kvs(ReqItem* reqs, ValItem* res, int size) {
    hash_init();
    for (int i = 0; i < size; i++) {
        if (reqs[i].op == 'I') {
            if(insert(reqs[i].key, reqs[i].value))
                printf("Insert (%d, %s)\n", reqs[i].key, reqs[i].value);
        }
        else {
            int v_addr = find(reqs[i].key);
            if (v_addr == -1) {
                printf("Search result: Not find\n");
                continue;
            }
            printf("Search result: (%d, %s)\n", reqs[i].key, values[v_addr].val);
            strcpy(res[i], values[v_addr].val);
        }
    }
}
}