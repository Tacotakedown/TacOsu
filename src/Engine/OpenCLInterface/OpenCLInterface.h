#ifndef OPENCLINTERFACE_H
#define OPENCLINTEFFACE_H

#include"Engine.h"

#include <CL/cl.h>

class OpenCLInterface {
public:
	struct OPENCL_DEVICE {
		UString name;
		UString vendor;
		UString version;
		UString profile;
		UString extensions;
	};

public:
	OpenCLInterface();
	~OpenCLInterface();

	template <typename T>
	int createBuffer(unsigned int numberOfObjects = 1, bool readable = true, bool writeable = true);
	template <typename T>
	void writeBuffer(int buffer, unsigned int numberOfObjects, const void* ptr);
	template <typename T>
	void writeBuffer(int buffer, unsigned int numberOfObjects, unsigned int startObjectIndex, const void* ptr);
	template <typename T>
	void readBuffer(int buffer, unsigned int numberOfObjects, void* ptr);
	void releaseBuffer(int buffer);
	template <typename T>
	void updateBuffer(int buffer, unsigned int numberOfObjects, bool readable, bool writeable);

	// OpenGL interop
	void aquireGLObject(int object);
	void releaseGLObject(int object);
	int createTexture(unsigned int sourceImage, bool readable = true, bool writeable = false);
	int createTexture3D(unsigned int sourceImage); // currently OpenCL 1.1. only supports native reads from 3d images

	// workgroup size
	int getWorkGroupSize(int kernel);

	// kernel
	int createKernel(UString kernelSourceCode, UString functionName);
	int createKernel(const char* kernelSourceCode, UString functionName);
	void setKernelArg(int kernel, unsigned int argumentNumber, int buffer);
	template <typename T>
	void setKernelArg(int kernel, unsigned int argumentNumber, T argument);

	void executeKernel(int kernel, unsigned int numLoops, const size_t* globalItemSize, const size_t* localItemSize);

	// misc
	void cleanup();
	void releaseKernel(int kernel);
	void flush();
	void finish();

	inline int getMaxMemAllocSizeInMB() const { return m_iMaxMemAllocSizeInMB; }
	inline int getGlobalMemSizeInMB() const { return m_iGlobalMemSizeInMB; }

	const std::vector<OPENCL_DEVICE>& getDevices() const { return m_devices; }


private:
	std::vector<OPENCL_DEVICE> m_devices;

	unsigned int m_iMaxMemAllocSizeInMB;
	unsigned int m_iGlobalMemSizeInMB;
	cl_device_id m_deviceID;
	cl_context m_context;
	cl_command_queue m_commandQueue;

	std::vector<cl_mem> m_vBuffers;
	std::vector<int> m_vBufferIndex;
	std::vector<cl_program> m_vPrograms;
	std::vector<cl_kernel> m_vKernels;
};

extern OpenCLInterface* opencl;

#endif // !OPENCLINTERFACE_H

//work on me please , we need buffers retard