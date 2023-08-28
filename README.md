vitis 实现 key value store <br/>
支持变长的key和size，哈希表采用十字链表法解决冲突，用数组模拟链表<br/>
哈希表从堆前面开始存，kv对从堆后面开始存 hp=>----------------<=kvp<br/>
其中哈希条目采用定长结构以支持索引访问。如果key长度过大，则只在哈希条目中存储标记，在查找时用来比较<br/>
所有结构flatten到一维数组再传送到kernel，通过指针强制转换和偏移量进行解析<br/>
哈希表节点结构：<br/>
```cpp
struct hItem {      // hash条目结构
    bool tag;       // tag=true, mk存标记，tag=false, mk存实际key
    union mItem {
        ull mark;
        char key[MarkSize];
    } mk;
    int kv_addr;    // kv对存储地址
    int next;       // 下一hash条目位置
};
```
kv条目结构<br/>
```cpp
struct kvItem {    // kv对条目结构
    int ksize;
    int vsize;
    char* key;
    char* value;
};
```
堆结构<br/>
```cpp
struct Heap {
    char heap[HEAP_SIZE];
    int hp, kvp;    // hp指向插入哈希条目位置，kvp指向插入kv条目位置 hp=>-------<=kvp
};
```
