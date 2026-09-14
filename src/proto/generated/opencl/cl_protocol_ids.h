// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.opencl --xml cl.xml --out ...

#pragma once

#include <stdint.h>

namespace av::clproto {

inline constexpr uint32_t kCmd_clGetPlatformIDs = 0x00020000U;
inline constexpr uint32_t kCmd_clGetPlatformInfo = 0x00020001U;
inline constexpr uint32_t kCmd_clGetDeviceIDs = 0x00020002U;
inline constexpr uint32_t kCmd_clGetDeviceInfo = 0x00020003U;
inline constexpr uint32_t kCmd_clCreateContext = 0x00020004U;
inline constexpr uint32_t kCmd_clCreateContextFromType = 0x00020005U;
inline constexpr uint32_t kCmd_clRetainContext = 0x00020006U;
inline constexpr uint32_t kCmd_clReleaseContext = 0x00020007U;
inline constexpr uint32_t kCmd_clGetContextInfo = 0x00020008U;
inline constexpr uint32_t kCmd_clCreateCommandQueue = 0x00020009U;
inline constexpr uint32_t kCmd_clCreateCommandQueueWithProperties = 0x0002000AU;
inline constexpr uint32_t kCmd_clRetainCommandQueue = 0x0002000BU;
inline constexpr uint32_t kCmd_clReleaseCommandQueue = 0x0002000CU;
inline constexpr uint32_t kCmd_clGetCommandQueueInfo = 0x0002000DU;
inline constexpr uint32_t kCmd_clFlush = 0x0002000EU;
inline constexpr uint32_t kCmd_clFinish = 0x0002000FU;
inline constexpr uint32_t kCmd_clCreateBuffer = 0x00020010U;
inline constexpr uint32_t kCmd_clCreateSubBuffer = 0x00020011U;
inline constexpr uint32_t kCmd_clCreateImage = 0x00020012U;
inline constexpr uint32_t kCmd_clRetainMemObject = 0x00020013U;
inline constexpr uint32_t kCmd_clReleaseMemObject = 0x00020014U;
inline constexpr uint32_t kCmd_clGetMemObjectInfo = 0x00020015U;
inline constexpr uint32_t kCmd_clGetImageInfo = 0x00020016U;
inline constexpr uint32_t kCmd_clGetSupportedImageFormats = 0x00020017U;
inline constexpr uint32_t kCmd_clEnqueueReadBuffer = 0x00020018U;
inline constexpr uint32_t kCmd_clEnqueueWriteBuffer = 0x00020019U;
inline constexpr uint32_t kCmd_clEnqueueCopyBuffer = 0x0002001AU;
inline constexpr uint32_t kCmd_clEnqueueFillBuffer = 0x0002001BU;
inline constexpr uint32_t kCmd_clEnqueueReadImage = 0x0002001CU;
inline constexpr uint32_t kCmd_clEnqueueWriteImage = 0x0002001DU;
inline constexpr uint32_t kCmd_clEnqueueCopyImage = 0x0002001EU;
inline constexpr uint32_t kCmd_clEnqueueCopyImageToBuffer = 0x0002001FU;
inline constexpr uint32_t kCmd_clEnqueueCopyBufferToImage = 0x00020020U;
inline constexpr uint32_t kCmd_clCreateProgramWithSource = 0x00020021U;
inline constexpr uint32_t kCmd_clCreateProgramWithBinary = 0x00020022U;
inline constexpr uint32_t kCmd_clRetainProgram = 0x00020023U;
inline constexpr uint32_t kCmd_clReleaseProgram = 0x00020024U;
inline constexpr uint32_t kCmd_clBuildProgram = 0x00020025U;
inline constexpr uint32_t kCmd_clGetProgramInfo = 0x00020026U;
inline constexpr uint32_t kCmd_clGetProgramBuildInfo = 0x00020027U;
inline constexpr uint32_t kCmd_clUnloadPlatformCompiler = 0x00020028U;
inline constexpr uint32_t kCmd_clCreateKernel = 0x00020029U;
inline constexpr uint32_t kCmd_clCreateKernelsInProgram = 0x0002002AU;
inline constexpr uint32_t kCmd_clRetainKernel = 0x0002002BU;
inline constexpr uint32_t kCmd_clReleaseKernel = 0x0002002CU;
inline constexpr uint32_t kCmd_clSetKernelArg = 0x0002002DU;
inline constexpr uint32_t kCmd_clGetKernelInfo = 0x0002002EU;
inline constexpr uint32_t kCmd_clGetKernelArgInfo = 0x0002002FU;
inline constexpr uint32_t kCmd_clGetKernelWorkGroupInfo = 0x00020030U;
inline constexpr uint32_t kCmd_clEnqueueNDRangeKernel = 0x00020031U;
inline constexpr uint32_t kCmd_clEnqueueMarkerWithWaitList = 0x00020032U;
inline constexpr uint32_t kCmd_clEnqueueBarrierWithWaitList = 0x00020033U;
inline constexpr uint32_t kCmd_clWaitForEvents = 0x00020034U;
inline constexpr uint32_t kCmd_clGetEventInfo = 0x00020035U;
inline constexpr uint32_t kCmd_clCreateUserEvent = 0x00020036U;
inline constexpr uint32_t kCmd_clSetUserEventStatus = 0x00020037U;
inline constexpr uint32_t kCmd_clRetainEvent = 0x00020038U;
inline constexpr uint32_t kCmd_clReleaseEvent = 0x00020039U;
inline constexpr uint32_t kCmd_clGetEventProfilingInfo = 0x0002003AU;
inline constexpr uint32_t kCmd_clCreateFromGLBuffer = 0x0002005CU;
inline constexpr uint32_t kCmd_clCreateFromGLTexture = 0x0002005DU;
inline constexpr uint32_t kCmd_clCreateFromGLTexture2D = 0x0002005EU;
inline constexpr uint32_t kCmd_clCreateFromGLTexture3D = 0x0002005FU;
inline constexpr uint32_t kCmd_clCreateFromGLRenderbuffer = 0x00020060U;
inline constexpr uint32_t kCmd_clGetGLObjectInfo = 0x00020061U;
inline constexpr uint32_t kCmd_clGetGLTextureInfo = 0x00020062U;
inline constexpr uint32_t kCmd_clEnqueueAcquireGLObjects = 0x00020063U;
inline constexpr uint32_t kCmd_clEnqueueReleaseGLObjects = 0x00020064U;
inline constexpr uint32_t kCmd_clGetGLContextInfoKHR = 0x00020065U;
inline constexpr uint32_t kCmd_clRetainDevice = 0x0002003FU;
inline constexpr uint32_t kCmd_clReleaseDevice = 0x00020040U;
inline constexpr uint32_t kCmd_clCreateSubDevices = 0x00020041U;
inline constexpr uint32_t kCmd_clEnqueueTask = 0x00020042U;
inline constexpr uint32_t kCmd_clEnqueueMarker = 0x00020043U;
inline constexpr uint32_t kCmd_clEnqueueBarrier = 0x00020044U;
inline constexpr uint32_t kCmd_clEnqueueWaitForEvents = 0x00020045U;
inline constexpr uint32_t kCmd_clEnqueueMigrateMemObjects = 0x00020046U;
inline constexpr uint32_t kCmd_clEnqueueReadBufferRect = 0x00020047U;
inline constexpr uint32_t kCmd_clEnqueueWriteBufferRect = 0x00020048U;
inline constexpr uint32_t kCmd_clEnqueueCopyBufferRect = 0x00020049U;
inline constexpr uint32_t kCmd_clEnqueueFillImage = 0x0002004AU;
inline constexpr uint32_t kCmd_clCreateImage2D = 0x0002004BU;
inline constexpr uint32_t kCmd_clCreateImage3D = 0x0002004CU;
inline constexpr uint32_t kCmd_clUnloadCompiler = 0x0002004DU;
inline constexpr uint32_t kCmd_clSetCommandQueueProperty = 0x0002004EU;
inline constexpr uint32_t kCmd_clCreateProgramWithIL = 0x0002004FU;
inline constexpr uint32_t kCmd_clCompileProgram = 0x00020050U;
inline constexpr uint32_t kCmd_clLinkProgram = 0x00020051U;
inline constexpr uint32_t kCmd_clSetProgramSpecializationConstant = 0x00020052U;
inline constexpr uint32_t kCmd_clCloneKernel = 0x00020053U;
inline constexpr uint32_t kCmd_clSetKernelExecInfo = 0x00020054U;
inline constexpr uint32_t kCmd_clGetKernelSubGroupInfo = 0x00020055U;
inline constexpr uint32_t kCmd_clGetHostTimer = 0x00020056U;
inline constexpr uint32_t kCmd_clGetDeviceAndHostTimer = 0x00020057U;
inline constexpr uint32_t kCmd_clCreateBufferWithProperties = 0x00020058U;
inline constexpr uint32_t kCmd_clCreateImageWithProperties = 0x00020059U;
inline constexpr uint32_t kCmd_clCreateSamplerWithProperties = 0x0002005AU;
inline constexpr uint32_t kCmd_clSetDefaultDeviceCommandQueue = 0x0002005BU;
inline constexpr uint32_t kCmd_clCreateSampler = 0x0002003BU;
inline constexpr uint32_t kCmd_clRetainSampler = 0x0002003CU;
inline constexpr uint32_t kCmd_clReleaseSampler = 0x0002003DU;
inline constexpr uint32_t kCmd_clGetSamplerInfo = 0x0002003EU;
inline constexpr uint32_t kCmd_clCreatePipe = 0x00020066U;
inline constexpr uint32_t kCmd_clGetPipeInfo = 0x00020067U;
inline constexpr uint32_t kCmd_clCreateProgramWithBuiltInKernels = 0x00020068U;

}  // namespace av::clproto
