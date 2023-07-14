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

using std::vector;

static const int DATA_SIZE = 2;
static const int ValueMaxSize = 8;

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

    OCL_CHECK(err, cl::Buffer buffer_op(context, CL_MEM_READ_ONLY, sizeof(char) * DATA_SIZE, nullptr, &err));
    OCL_CHECK(err, cl::Buffer buffer_key(context, CL_MEM_READ_ONLY, sizeof(int) * DATA_SIZE, nullptr, &err));
    // how to define the size of value and res?
    OCL_CHECK(err, cl::Buffer buffer_value(context, CL_MEM_READ_ONLY, sizeof(char) * DATA_SIZE * ValueMaxSize, nullptr, &err));
    OCL_CHECK(err, cl::Buffer buffer_res(context, CL_MEM_WRITE_ONLY, sizeof(char) * DATA_SIZE * ValueMaxSize, nullptr, &err));

    OCL_CHECK(err, kernel.setArg(0, buffer_op));
    OCL_CHECK(err, kernel.setArg(1, buffer_key));
    OCL_CHECK(err, kernel.setArg(2, buffer_value));
    OCL_CHECK(err, kernel.setArg(3, buffer_res));
    OCL_CHECK(err, kernel.setArg(4, DATA_SIZE));

    char* ptr_op = (char*)q.enqueueMapBuffer(buffer_op, CL_TRUE, CL_MAP_WRITE, 0, sizeof(char) * DATA_SIZE);
    int* ptr_key = (int*)q.enqueueMapBuffer(buffer_key, CL_TRUE, CL_MAP_WRITE, 0, sizeof(int) * DATA_SIZE);
    // how to define the size of value and res?
    char* ptr_value = (char*)q.enqueueMapBuffer(buffer_value, CL_TRUE, CL_MAP_WRITE, 0, sizeof(char) * DATA_SIZE * ValueMaxSize);
    char* ptr_res = (char*)q.enqueueMapBuffer(buffer_res, CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, sizeof(char) * DATA_SIZE * ValueMaxSize);

    memset(ptr_res, '\0', sizeof(char) * DATA_SIZE * ValueMaxSize);
    ptr_op[0] = 'I';
    ptr_key[0] = 1;
    strcpy(ptr_value + 0 * ValueMaxSize, "hello");
    ptr_op[1] = 'S';
    ptr_key[1] = 1;

    q.enqueueMigrateMemObjects({buffer_op, buffer_key, buffer_value}, 0);
    q.enqueueTask(kernel);
    q.enqueueMigrateMemObjects({buffer_res}, CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();

    bool match = true;
    std::unordered_map<int, char*> umap;
    for (int i = 0; i < DATA_SIZE; i++) {
        if (ptr_op[i] == 'I') umap[ptr_key[i]] = ptr_value + i * ValueMaxSize;
        char* host_result = nullptr;
        if (ptr_op[i] == 'S') host_result = umap[ptr_key[i]];
        if (ptr_op[i] == 'S' && strcmp(host_result, ptr_res + i * ValueMaxSize)) {
            std::cout << host_result << " " << ptr_res + i * ValueMaxSize << std::endl;
            printf(error_message.c_str(), i, host_result, ptr_res + i * ValueMaxSize);
            match = false;
            break;
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
