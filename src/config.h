#ifndef _KVS_CONFIG_H_
#define _KVS_CONFIG_H_

#define BucketNum 13
#define HashItemNum BucketNum * 4
#define kvNum HashItemNum
#define MarkSize 8

typedef long long ll;
typedef unsigned long long ull;


struct hItem {      // hash条目结构
    bool tag;       // tag=true, mk存标记，tag=false, mk存实际key
    union mItem {
        ull mark;
        char key[MarkSize];
    } mk;
    int kv_addr;    // kv对存储地址
    int next;       // 下一hash条目位置
};

struct kvItem {    // kv对条目结构
    int ksize;
    int vsize;
    char* key;
    char* value;
};

struct vItem {
    int vsize;
    char* value;
};

struct ReqItem {    // 请求条目
    char op;    // 操作类型 I：插入 S：查询
    int ksize;
    int vsize;
    char* key;
    char* value;
};

struct Heap {
    char* heap;
    int hp, kvp;
};

#endif