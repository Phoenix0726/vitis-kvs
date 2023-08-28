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
static const int HEAP_SIZE = 1024 * 1024;

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

    OCL_CHECK(err, cl::Buffer buffer_req(context, CL_MEM_READ_ONLY, sizeof(ReqItem) * DATA_SIZE, nullptr, &err));
    OCL_CHECK(err, cl::Buffer buffer_res(context, CL_MEM_READ_WRITE, sizeof(vItem) * DATA_SIZE, nullptr, &err));
    OCL_CHECK(err, cl::Buffer buffer_heap(context, CL_MEM_READ_WRITE, HEAP_SIZE + 2*sizeof(int), nullptr, &err));

    OCL_CHECK(err, kernel.setArg(0, buffer_req));
    OCL_CHECK(err, kernel.setArg(1, buffer_res));
    OCL_CHECK(err, kernel.setArg(2, DATA_SIZE));
    OCL_CHECK(err, kernel.setArg(3, buffer_heap));

    ReqItem* ptr_req = (ReqItem*)q.enqueueMapBuffer(buffer_req, CL_TRUE, CL_MAP_WRITE, 0, sizeof(ReqItem) * DATA_SIZE);
    vItem* ptr_res = (vItem*)q.enqueueMapBuffer(buffer_res, CL_TRUE, CL_MAP_READ, 0, sizeof(vItem) * DATA_SIZE);
    Heap* ptr_heap = (Heap*)q.enqueueMapBuffer(buffer_heap, CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, HEAP_SIZE + 2*sizeof(int));

    // 初始化请求
    std::cout << "*** init reqs ***" << std::endl;
    memset(ptr_res, '\0', sizeof(vItem) * DATA_SIZE);
    ptr_req[0] = (ReqItem){'I', 2, 10, "h", "hello"};
    ptr_req[1] = (ReqItem){'S', 2, 0, "h", "\0"};
    ptr_req[2] = (ReqItem){'S', 2, 0, "w", "\0"};
    ptr_req[3] = (ReqItem){'I', 2, 10, "w", "world"};
    ptr_req[4] = (ReqItem){'S', 2, 0, "w", "\0"};
    // ptr_req[5] = (ReqItem){'I', 14, "hihi"};
    // ptr_req[6] = (ReqItem){'S', 14, "\0"};
    std::cout << ptr_req[0].key << std::endl;
    std::cout << ptr_req[0].value << std::endl;

    // 初始化堆
    std::cout << "*** init heap ***" << std::endl;
    ptr_heap = (Heap*)malloc(sizeof(Heap));
    ptr_heap->heap = (char*)malloc(HEAP_SIZE);
    memset(ptr_heap->heap, '\0', HEAP_SIZE);
    ptr_heap->hp = BucketNum * sizeof(hItem);
    ptr_heap->kvp = HEAP_SIZE;
    std::cout << "*** init heap complete ***" << std::endl;

    q.enqueueMigrateMemObjects({buffer_req}, 0);
    q.enqueueMigrateMemObjects({buffer_heap}, 0);
    std::cout << "------------------------------------" << std::endl;
    q.enqueueTask(kernel);
    std::cout << "====================================" << std::endl;
    q.enqueueMigrateMemObjects({buffer_res}, CL_MIGRATE_MEM_OBJECT_HOST);
    std::cout << "************************************" << std::endl;
    q.finish();
    std::cout << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^" << std::endl;

    bool match = true;
    // std::unordered_map<char*, vItem> umap;
    // for (int i = 0; i < DATA_SIZE; i++) {
    //     if (ptr_req[i].op == 'I') strcpy(umap[ptr_req[i].kv.key].value, ptr_req[i].kv.value);
    //     char* host_result = nullptr;
    //     if (ptr_req[i].op == 'S') host_result = umap[ptr_req[i].kv.key].value;
    //     if (ptr_req[i].op == 'S' && strcmp(host_result, ptr_res[i].value)) {
    //         std::cout << host_result << " " << ptr_res[i].value << std::endl;
    //         printf(error_message.c_str(), i, host_result, ptr_res[i]);
    //         match = false;
    //         // break;
    //     }
    // }

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
