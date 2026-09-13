// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.opencl --xml cl.xml --out ...

#include "cl_command_names.h"

#include "cl_protocol_ids.h"

namespace av::clproto {

const char* cmd_id_to_string(std::uint32_t cmd_id) noexcept {
    switch (cmd_id) {
        case kCmd_clGetPlatformIDs: return "clGetPlatformIDs";
        case kCmd_clGetPlatformInfo: return "clGetPlatformInfo";
        case kCmd_clGetDeviceIDs: return "clGetDeviceIDs";
        case kCmd_clGetDeviceInfo: return "clGetDeviceInfo";
        case kCmd_clCreateContext: return "clCreateContext";
        case kCmd_clCreateContextFromType: return "clCreateContextFromType";
        case kCmd_clRetainContext: return "clRetainContext";
        case kCmd_clReleaseContext: return "clReleaseContext";
        case kCmd_clGetContextInfo: return "clGetContextInfo";
        case kCmd_clCreateCommandQueue: return "clCreateCommandQueue";
        case kCmd_clCreateCommandQueueWithProperties: return "clCreateCommandQueueWithProperties";
        case kCmd_clRetainCommandQueue: return "clRetainCommandQueue";
        case kCmd_clReleaseCommandQueue: return "clReleaseCommandQueue";
        case kCmd_clGetCommandQueueInfo: return "clGetCommandQueueInfo";
        case kCmd_clFlush: return "clFlush";
        case kCmd_clFinish: return "clFinish";
        case kCmd_clCreateBuffer: return "clCreateBuffer";
        case kCmd_clCreateSubBuffer: return "clCreateSubBuffer";
        case kCmd_clCreateImage: return "clCreateImage";
        case kCmd_clRetainMemObject: return "clRetainMemObject";
        case kCmd_clReleaseMemObject: return "clReleaseMemObject";
        case kCmd_clGetMemObjectInfo: return "clGetMemObjectInfo";
        case kCmd_clGetImageInfo: return "clGetImageInfo";
        case kCmd_clGetSupportedImageFormats: return "clGetSupportedImageFormats";
        case kCmd_clEnqueueReadBuffer: return "clEnqueueReadBuffer";
        case kCmd_clEnqueueWriteBuffer: return "clEnqueueWriteBuffer";
        case kCmd_clEnqueueCopyBuffer: return "clEnqueueCopyBuffer";
        case kCmd_clEnqueueFillBuffer: return "clEnqueueFillBuffer";
        case kCmd_clEnqueueReadImage: return "clEnqueueReadImage";
        case kCmd_clEnqueueWriteImage: return "clEnqueueWriteImage";
        case kCmd_clEnqueueCopyImage: return "clEnqueueCopyImage";
        case kCmd_clEnqueueCopyImageToBuffer: return "clEnqueueCopyImageToBuffer";
        case kCmd_clEnqueueCopyBufferToImage: return "clEnqueueCopyBufferToImage";
        case kCmd_clCreateProgramWithSource: return "clCreateProgramWithSource";
        case kCmd_clCreateProgramWithBinary: return "clCreateProgramWithBinary";
        case kCmd_clRetainProgram: return "clRetainProgram";
        case kCmd_clReleaseProgram: return "clReleaseProgram";
        case kCmd_clBuildProgram: return "clBuildProgram";
        case kCmd_clGetProgramInfo: return "clGetProgramInfo";
        case kCmd_clGetProgramBuildInfo: return "clGetProgramBuildInfo";
        case kCmd_clUnloadPlatformCompiler: return "clUnloadPlatformCompiler";
        case kCmd_clCreateKernel: return "clCreateKernel";
        case kCmd_clCreateKernelsInProgram: return "clCreateKernelsInProgram";
        case kCmd_clRetainKernel: return "clRetainKernel";
        case kCmd_clReleaseKernel: return "clReleaseKernel";
        case kCmd_clSetKernelArg: return "clSetKernelArg";
        case kCmd_clGetKernelInfo: return "clGetKernelInfo";
        case kCmd_clGetKernelArgInfo: return "clGetKernelArgInfo";
        case kCmd_clGetKernelWorkGroupInfo: return "clGetKernelWorkGroupInfo";
        case kCmd_clEnqueueNDRangeKernel: return "clEnqueueNDRangeKernel";
        case kCmd_clEnqueueMarkerWithWaitList: return "clEnqueueMarkerWithWaitList";
        case kCmd_clEnqueueBarrierWithWaitList: return "clEnqueueBarrierWithWaitList";
        case kCmd_clWaitForEvents: return "clWaitForEvents";
        case kCmd_clGetEventInfo: return "clGetEventInfo";
        case kCmd_clCreateUserEvent: return "clCreateUserEvent";
        case kCmd_clSetUserEventStatus: return "clSetUserEventStatus";
        case kCmd_clRetainEvent: return "clRetainEvent";
        case kCmd_clReleaseEvent: return "clReleaseEvent";
        case kCmd_clGetEventProfilingInfo: return "clGetEventProfilingInfo";
        case kCmd_clCreateFromGLBuffer: return "clCreateFromGLBuffer";
        case kCmd_clCreateFromGLTexture: return "clCreateFromGLTexture";
        case kCmd_clCreateFromGLTexture2D: return "clCreateFromGLTexture2D";
        case kCmd_clCreateFromGLTexture3D: return "clCreateFromGLTexture3D";
        case kCmd_clCreateFromGLRenderbuffer: return "clCreateFromGLRenderbuffer";
        case kCmd_clGetGLObjectInfo: return "clGetGLObjectInfo";
        case kCmd_clGetGLTextureInfo: return "clGetGLTextureInfo";
        case kCmd_clEnqueueAcquireGLObjects: return "clEnqueueAcquireGLObjects";
        case kCmd_clEnqueueReleaseGLObjects: return "clEnqueueReleaseGLObjects";
        case kCmd_clGetGLContextInfoKHR: return "clGetGLContextInfoKHR";
        case kCmd_clRetainDevice: return "clRetainDevice";
        case kCmd_clReleaseDevice: return "clReleaseDevice";
        case kCmd_clCreateSubDevices: return "clCreateSubDevices";
        case kCmd_clEnqueueTask: return "clEnqueueTask";
        case kCmd_clEnqueueMarker: return "clEnqueueMarker";
        case kCmd_clEnqueueBarrier: return "clEnqueueBarrier";
        case kCmd_clEnqueueWaitForEvents: return "clEnqueueWaitForEvents";
        case kCmd_clEnqueueMigrateMemObjects: return "clEnqueueMigrateMemObjects";
        case kCmd_clEnqueueReadBufferRect: return "clEnqueueReadBufferRect";
        case kCmd_clEnqueueWriteBufferRect: return "clEnqueueWriteBufferRect";
        case kCmd_clEnqueueCopyBufferRect: return "clEnqueueCopyBufferRect";
        case kCmd_clEnqueueFillImage: return "clEnqueueFillImage";
        case kCmd_clCreateImage2D: return "clCreateImage2D";
        case kCmd_clCreateImage3D: return "clCreateImage3D";
        case kCmd_clUnloadCompiler: return "clUnloadCompiler";
        case kCmd_clSetCommandQueueProperty: return "clSetCommandQueueProperty";
        case kCmd_clCreateProgramWithIL: return "clCreateProgramWithIL";
        case kCmd_clCompileProgram: return "clCompileProgram";
        case kCmd_clLinkProgram: return "clLinkProgram";
        case kCmd_clSetProgramSpecializationConstant: return "clSetProgramSpecializationConstant";
        case kCmd_clCloneKernel: return "clCloneKernel";
        case kCmd_clSetKernelExecInfo: return "clSetKernelExecInfo";
        case kCmd_clGetKernelSubGroupInfo: return "clGetKernelSubGroupInfo";
        case kCmd_clGetHostTimer: return "clGetHostTimer";
        case kCmd_clGetDeviceAndHostTimer: return "clGetDeviceAndHostTimer";
        case kCmd_clCreateBufferWithProperties: return "clCreateBufferWithProperties";
        case kCmd_clCreateImageWithProperties: return "clCreateImageWithProperties";
        case kCmd_clCreateSamplerWithProperties: return "clCreateSamplerWithProperties";
        case kCmd_clSetDefaultDeviceCommandQueue: return "clSetDefaultDeviceCommandQueue";
        case kCmd_clCreateSampler: return "clCreateSampler";
        case kCmd_clRetainSampler: return "clRetainSampler";
        case kCmd_clReleaseSampler: return "clReleaseSampler";
        case kCmd_clGetSamplerInfo: return "clGetSamplerInfo";
        case kCmd_clCreatePipe: return "clCreatePipe";
        case kCmd_clGetPipeInfo: return "clGetPipeInfo";
        case kCmd_clCreateProgramWithBuiltInKernels: return "clCreateProgramWithBuiltInKernels";
        default: return "unknown";
    }
}

}  // namespace av::clproto
