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

#include "config.h"

using std::vector;

static const int DATA_SIZE = 5;
static const int REQ_SIZE = 1024 * 1024;
static const int RES_SIZE = 1024 * 1024;

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
    OCL_CHECK(err, kernel.setArg(2, DATA_SIZE));
    OCL_CHECK(err, kernel.setArg(3, buffer_heap));

    char* ptr_req = (char*)q.enqueueMapBuffer(buffer_req, CL_TRUE, CL_MAP_WRITE, 0, REQ_SIZE);
    char* ptr_res = (char*)q.enqueueMapBuffer(buffer_res, CL_TRUE, CL_MAP_READ, 0, RES_SIZE);
    char* ptr_heap = (char*)q.enqueueMapBuffer(buffer_heap, CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, HEAP_SIZE);

    // 初始化请求
    std::cout << "*** init reqs ***" << std::endl;
    ReqItem reqs[DATA_SIZE];
    reqs[0] = (ReqItem){'I', 2, 10, "h", "hello"};
    reqs[1] = (ReqItem){'S', 2, 0, "h", ""};
    reqs[2] = (ReqItem){'S', 2, 0, "w", ""};
    reqs[3] = (ReqItem){'I', 2, 10, "w", "world"};
    reqs[4] = (ReqItem){'S', 2, 0, "w", ""};
    // ptr_req[5] = (ReqItem){'I', 14, "hihi"};
    // ptr_req[6] = (ReqItem){'S', 14, "\0"};
    // 把请求flatten到char数组中
    char* reqp = ptr_req;
    for (int i = 0; i < DATA_SIZE; i++) {
        *reqp = reqs[i].op;
        *(int*)(reqp + 1) = reqs[i].ksize;
        *(int*)(reqp + 5) = reqs[i].vsize;
        strncpy(reqp + 9, reqs[i].key, reqs[i].ksize);
        strncpy(reqp + 9 + reqs[i].ksize, reqs[i].value, reqs[i].vsize);
        reqp += 9 + reqs[i].ksize + reqs[i].vsize;
    }
    std::cout << "*** init reqs complete ***" << std::endl;

    // 初始化堆
    std::cout << "*** init heap ***" << std::endl;
    memset(ptr_heap + 2 * sizeof(int), '\0', HEAP_SIZE);
    *(int*)ptr_heap = 2 * sizeof(int) + BucketNum * sizeof(hItem);
    *(int*)(ptr_heap + sizeof(int)) = HEAP_SIZE;
    // 初始化哈希表
    hItem* hTable = (hItem*)(ptr_heap + 2 * sizeof(int));
    for (int i = 0; i < BucketNum; i++) {
        hTable[i].next = -1;
    }
    std::cout << "*** init heap complete ***" << std::endl;

    memset(ptr_res, '\0', RES_SIZE);

    q.enqueueMigrateMemObjects({buffer_req}, 0);
    q.enqueueMigrateMemObjects({buffer_heap}, 0);
    q.enqueueTask(kernel);
    q.enqueueMigrateMemObjects({buffer_res}, CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();

    bool match = true;
    std::unordered_map<char*, char*> umap;
    for (int i = 0; i < DATA_SIZE; i++) {
        if (reqs[i].op == 'I') {
            // strcpy(umap[reqs[i].key], reqs[i].value);
            umap[reqs[i].key] = reqs[i].value;
        }
        if (reqs[i].op == 'S') {
            char* host_result = nullptr;
            host_result = umap[reqs[i].key];
            if (host_result && strncmp(host_result, ptr_res + sizeof(int), *(int*)ptr_res)) {
                std::cout << host_result << " " << ptr_res << std::endl;
                printf(error_message.c_str(), i, host_result, ptr_res);
                match = false;
                break;
            }
            ptr_res += sizeof(int) + *(int*)ptr_res;
        }
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
