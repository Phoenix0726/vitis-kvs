#define OCL_CHECK(error, call)                                                                      \
    call;                                                                                           \
    if (error != CL_SUCCESS) {                                                                      \
        printf("%s:%d Error calling " #call ", error code is: %d\n", __FILE__, __LINE__, error);    \
        exit(EXIT_FAILURE);                                                                         \
    }

#define CL_HPP_CL_1_2_DEFAULT_BUILD
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_PROGRAM_CONSTRUCTION_FROM_ARRAY_COMPATIBILITY 1
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

#include <CL/cl2.hpp>
#include <vector>
#include <unistd.h>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <ctime>
#include <sys/time.h>

#include "config.h"

using std::vector;

static const int DATA_SIZE = 10000000;
static const int REQ_SIZE = 128 * DATA_SIZE;
static const int RES_SIZE = 128 * DATA_SIZE;
static const int BucketNum = 900001;

static const std::string error_message = 
    "Error: Result mismatch:\n"
    "i = %d CPU result = %d Device result = %d\n";

std::vector<cl::Device> get_xilinx_devices();
char *read_binary_file(const std::string &xclbin_file_name, unsigned &nb);


int main(int argc, char** argv)
{
    cl_int err;
    std::string binaryFile = (argc != 2) ? "vadd.xclbin" : argv[1];
    unsigned fileBufSize;
    std::vector<cl::Device> devices = get_xilinx_devices();
    devices.resize(1);
    cl::Device device = devices[0];
    cl::Context context(device, NULL, NULL, NULL, &err);
    char *fileBuf = read_binary_file(binaryFile, fileBufSize);
    cl::Program::Binaries bins{{fileBuf, fileBufSize}};
    cl::Program program(context, devices, bins, NULL, &err);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
    cl::Kernel kernel(program, "krnl_kvs", &err);

    OCL_CHECK(err, cl::Buffer buffer_req(context, CL_MEM_READ_ONLY, REQ_SIZE, nullptr, &err));
    OCL_CHECK(err, cl::Buffer buffer_res(context, CL_MEM_READ_WRITE, RES_SIZE, nullptr, &err));
    OCL_CHECK(err, cl::Buffer buffer_heap(context, CL_MEM_READ_WRITE, HEAP_SIZE, nullptr, &err));

    OCL_CHECK(err, kernel.setArg(0, buffer_req));
    OCL_CHECK(err, kernel.setArg(1, buffer_res));
    OCL_CHECK(err, kernel.setArg(2, buffer_heap));
    OCL_CHECK(err, kernel.setArg(3, DATA_SIZE));
    OCL_CHECK(err, kernel.setArg(4, BucketNum));

    char* ptr_req = (char*)q.enqueueMapBuffer(buffer_req, CL_TRUE, CL_MAP_WRITE, 0, REQ_SIZE);
    char* ptr_res = (char*)q.enqueueMapBuffer(buffer_res, CL_TRUE, CL_MAP_READ, 0, RES_SIZE);
    char* ptr_heap = (char*)q.enqueueMapBuffer(buffer_heap, CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, HEAP_SIZE);

    // 初始化请求
    std::cout << "*** init reqs ***" << std::endl;
    std::ifstream fin("../dataset/query10M/requests.dat");
    std::string line;
    int batchSize = 0;
    char* reqp = ptr_req;
    while (getline(fin, line)) {
        char op = line[0];
        if (op == 'U') continue;    // 暂时还处理不了update操作
        std::string key = line.substr(2, 24);
        std::string value = (op == 'I') ? line.substr(34) : "";
        int ksize = key.size() + 1;
        int vsize = (op == 'I') ? value.size() + 1 : 0;
        *reqp = op;                    // op
        *(int*)(reqp + 1) = ksize;     // ksize
        *(int*)(reqp + 5) = vsize;     // vsize
        strncpy(reqp + 9, key.c_str(), ksize);                  // key
        strncpy(reqp + 9 + ksize, value.c_str(), vsize);        // value
        // std::cout << *reqp << std::endl;
        // std::cout << *(int*)(reqp + 1) << std::endl;
        // std::cout << *(int*)(reqp + 5) << std::endl;
        // std::cout << reqp + 9 << std::endl;
        // std::cout << reqp + 9 + ksize << std::endl;
        reqp += 9 + ksize + vsize;
        batchSize++;
    }
    fin.close();
    OCL_CHECK(err, kernel.setArg(3, batchSize));
    std::cout << "*** init reqs complete ***" << std::endl;

    // 初始化堆
    std::cout << "*** init heap ***" << std::endl;
    memset(ptr_heap, '\0', HEAP_SIZE);
    *(int*)ptr_heap = 2 * sizeof(int) + BucketNum * sizeof(hItem);
    *(int*)(ptr_heap + sizeof(int)) = HEAP_SIZE;
    // 初始化哈希表
    hItem* hTable = (hItem*)(ptr_heap + 2 * sizeof(int));
    for (int i = 0; i < BucketNum; i++) {
        hTable[i].next = -1;
    }
    std::cout << "*** init heap complete ***" << std::endl;

    memset(ptr_res, '\0', RES_SIZE);

    // time_t start_t = time(NULL);
    // clock_t start_t, end_t;
    // start_t = clock();
    struct timeval start_t, end_t;
    gettimeofday(&start_t, NULL);
    std::cout << "Kernel start..." << std::endl;
    q.enqueueMigrateMemObjects({buffer_req}, 0);
    q.enqueueMigrateMemObjects({buffer_heap}, 0);
    q.enqueueTask(kernel);
    q.enqueueMigrateMemObjects({buffer_res}, CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();
    std::cout << "Kernel end" << std::endl;
    // time_t end_t = time(NULL);
    // end_t = clock();
    gettimeofday(&end_t, NULL);
    // std::cout << "*** Kernel execution time: " << difftime(end_t, start_t) << " ***" << std::endl;
    // std::cout << "*** Kernel execution time: " << double(end_t - start_t) / CLOCKS_PER_SEC << std::endl;
    double timeuse = (end_t.tv_sec - start_t.tv_sec) + (double)(end_t.tv_usec - start_t.tv_usec) / 1e6;
    std::cout << "*** Kernel execution time: " << timeuse << " ***" << std::endl;

    bool match = true;
    std::unordered_map<char*, char*> umap;
    reqp = ptr_req;
    char* resp = ptr_res;
    for (int i = 0; i < batchSize; i++) {
        if (*reqp == 'I') {     // req[i].op
            // strcpy(umap[reqs[i].key], reqs[i].value);
            umap[reqp + 9] = reqp + 9 + *(int*)(reqp + 1);      // umap[key] = value
        }
        if (*reqp == 'S') {
            char* host_result = nullptr;
            host_result = umap[reqp + 9];       // umap[key]
            if (host_result && strncmp(host_result, resp + sizeof(int), *(int*)resp)) {
                std::cout << host_result << " " << resp + sizeof(int) << std::endl;
                printf(error_message.c_str(), i, host_result, resp + sizeof(int));
                match = false;
                break;
            }
            resp += sizeof(int) + *(int*)resp;      // resp += 4 + vsize
        }
        reqp += 9 + *(int*)(reqp + 1) + *(int*)(reqp + 5);      // reqp += 9 + ksize + vsize
    }
    std::cout << "TEST " << (match ? "PASSED" : "FAILED") << std::endl;
    return (match ? EXIT_SUCCESS : EXIT_FAILURE);
}

std::vector<cl::Device> get_xilinx_devices()
{
    size_t i;
    cl_int err;
    std::vector<cl::Platform> platforms;
    err = cl::Platform::get(&platforms);
    cl::Platform platform;
    for (i = 0; i < platforms.size(); i++)
    {
        platform = platforms[i];
        std::string platformName = platform.getInfo<CL_PLATFORM_NAME>(&err);
        if (platformName == "Xilinx")
        {
            std::cout << "INFO: Found Xilinx Platform" << std::endl;
            break;
        }
    }
    if (i == platforms.size())
    {
        std::cout << "ERROR: Failed to find Xilinx platform" << std::endl;
        exit(EXIT_FAILURE);
    }

    //Getting ACCELERATOR Devices and selecting 1st such device
    std::vector<cl::Device> devices;
    err = platform.getDevices(CL_DEVICE_TYPE_ACCELERATOR, &devices);
    return devices;
}

char *read_binary_file(const std::string &xclbin_file_name, unsigned &nb)
{
    if (access(xclbin_file_name.c_str(), R_OK) != 0)
    {
        printf("ERROR: %s xclbin not available please build\n", xclbin_file_name.c_str());
        exit(EXIT_FAILURE);
    }
    //Loading XCL Bin into char buffer
    std::cout << "INFO: Loading '" << xclbin_file_name << "'\n";
    std::ifstream bin_file(xclbin_file_name.c_str(), std::ifstream::binary);
    bin_file.seekg(0, bin_file.end);
    nb = bin_file.tellg();
    bin_file.seekg(0, bin_file.beg);
    char *buf = new char[nb];
    bin_file.read(buf, nb);
    return buf;
}
