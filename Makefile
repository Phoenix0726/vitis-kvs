TARGET := hw
PLATFORM ?= xilinx_u250_gen3x16_xdma_3_1_202020_1
SRCDIR := src
BUILD_DIR := $(TARGET)
CXXFLAGS += -g -std=c++17 -Wall -O0

run: build
ifeq ($(TARGET), hw)
	cp xrt.ini $(BUILD_DIR)
	cd $(BUILD_DIR) && ./app.exe krnl_kvs.xclbin
else
	cp xrt.ini $(BUILD_DIR)
	cd $(BUILD_DIR) && export XCL_EMULATION_MODE=$(TARGET) && ./app.exe krnl_kvs.xclbin
endif

build: host emconfig xclbin

host: $(BUILD_DIR)/app.exe
$(BUILD_DIR)/app.exe:
	mkdir -p $(BUILD_DIR)
	g++ $(CXXFLAGS) $(SRCDIR)/host.cpp -o $(BUILD_DIR)/app.exe \
		-I${XILINX_XRT}/include/ \
		-L${XILINX_XRT}/lib -lOpenCL -lrt -pthread

xclbin: $(BUILD_DIR)/krnl_kvs.xclbin
$(BUILD_DIR)/krnl_kvs.xclbin: $(BUILD_DIR)/krnl_kvs.xo
	v++ -l -t ${TARGET} --platform $(PLATFORM) --config $(SRCDIR)/u250.cfg $(BUILD_DIR)/krnl_kvs.xo -o $(BUILD_DIR)/krnl_kvs.xclbin

xo: $(BUILD_DIR)/krnl_kvs.xo
$(BUILD_DIR)/krnl_kvs.xo:
	v++ -c -t ${TARGET} --platform $(PLATFORM) --config $(SRCDIR)/u250.cfg -k krnl_kvs -I$(SRCDIR) $(SRCDIR)/krnl.cpp -o $(BUILD_DIR)/krnl_kvs.xo

emconfig: $(BUILD_DIR)/emconfig.json
$(BUILD_DIR)/emconfig.json:
	emconfigutil --platform $(PLATFORM) --od $(BUILD_DIR) --nd 1

clean:
	rm -rf $(BUILD_DIR) vadd* app.exe *json opencl* *log *summary _x xilinx* .run .Xil .ipcache *.jou
