#include "guest/suite/opencl_scenario.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"

namespace av {

namespace {

/* Short enough that the program's command - source, count and handles - stays
 * far under ScenarioTransport::command_bytes. */
constexpr char vector_add_source[] =
    "__kernel void vector_add(__global const float *a, __global const float *b, __global float *c)\n"
    "{\n"
    "    size_t i = get_global_id(0);\n"
    "    c[i] = a[i] + b[i];\n"
    "}\n";
constexpr char vector_add_name[] = "vector_add";
constexpr cl_mem_flags copied_input = CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR;
constexpr cl_mem_flags written_output = CL_MEM_WRITE_ONLY;
/* What an error output holds until an answer overwrites it, so a call whose
 * answer never came fails rather than reading as CL_SUCCESS. */
constexpr cl_int unanswered = CL_OUT_OF_RESOURCES;

}  // namespace

OpenClScenario::OpenClScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : steps_(kmt, report, "opencl"), link_(kmt, search, steps_, SubApi::opencl), device_(0u), context_(0u),
      queue_(0u), buffers_{}, program_(0u), kernel_(0u), release_error_(CL_SUCCESS), inputs_{}, output_{}
      {
          AV_SCOPE("OpenClScenario::OpenClScenario");
      }

void OpenClScenario::run()
{
    AV_SCOPE("OpenClScenario::run");
    ScopeExit closing([&] { link_.close(); });
    if (link_.open() != Status::ok) return;
    Status built = find_device();
    if (built == Status::ok) built = make_queue();
    if (built == Status::ok) built = make_buffers();
    if (built == Status::ok) built = make_kernel();
    if (built == Status::ok) built = compute();
    release();
    if (built == Status::ok) static_cast<void>(steps_.check_result("release", release_error_));
}

/* The host shows a context its GPU's platform alone, but the search is the one
 * an application makes - the first GPU any platform offers - so it holds
 * wherever a CPU runtime or another GPU is listed too. */
Status OpenClScenario::find_device()
{
    AV_SCOPE("OpenClScenario::find_device");
    KmtTransport &transport = link_.transport();
    uint64_t      platforms[max_platforms] = {};
    cl_uint       count = 0u;
    const cl_int  listed = clproto::av_clGetPlatformIDs(transport, max_platforms, platforms, &count);
    if (listed != CL_SUCCESS) return steps_.check_result("platform", listed);
    const Status any = steps_.check_measured("platform", count != 0u ? Status::ok : Status::not_found, count);
    if (any != Status::ok) return any;
    const cl_uint searched = count < max_platforms ? count : max_platforms;
    cl_int        found = CL_DEVICE_NOT_FOUND;
    for (cl_uint index = 0u; index < searched && found != CL_SUCCESS; index++) {
        cl_uint devices = 0u;
        found = clproto::av_clGetDeviceIDs(transport, platforms[index], CL_DEVICE_TYPE_GPU, 1u, &device_, &devices);
    }
    return steps_.check_result("device", found);
}

Status OpenClScenario::make_queue()
{
    AV_SCOPE("OpenClScenario::make_queue");
    KmtTransport &transport = link_.transport();
    cl_int        error = unanswered;
    context_ = clproto::av_clCreateContext(transport, nullptr, 1u, &device_, &error);
    const Status made = steps_.check_result("context", error);
    if (made != Status::ok) return made;
    error = unanswered;
    queue_ = clproto::av_clCreateCommandQueueWithProperties(transport, context_, device_, nullptr, &error);
    return steps_.check_result("queue", error);
}

Status OpenClScenario::make_buffers()
{
    AV_SCOPE("OpenClScenario::make_buffers");
    for (uint32_t index = 0u; index < elements; index++) {
        inputs_[0][index] = static_cast<float>(index);
        inputs_[1][index] = static_cast<float>(2u * index);
    }
    KmtTransport &transport = link_.transport();
    cl_int        error = CL_SUCCESS;
    for (uint32_t index = 0u; index < buffer_count && error == CL_SUCCESS; index++) {
        const bool input = index < input_count;
        error = unanswered;
        buffers_[index] = clproto::av_clCreateBuffer(
            transport, context_, input ? copied_input : written_output,
            sizeof output_, input ? inputs_[index] : nullptr, &error);
    }
    return steps_.check_result("buffers", error);
}

Status OpenClScenario::make_kernel()
{
    AV_SCOPE("OpenClScenario::make_kernel");
    KmtTransport &transport = link_.transport();
    const char   *sources[] = {vector_add_source};
    cl_int        error = unanswered;
    program_ = clproto::av_clCreateProgramWithSource(transport, context_, 1u, sources, nullptr, &error);
    if (error == CL_SUCCESS) error = clproto::av_clBuildProgram(transport, program_, 1u, &device_, nullptr);
    const Status built = steps_.check_result("program", error);
    if (built != Status::ok) return built;
    error = unanswered;
    kernel_ = clproto::av_clCreateKernel(transport, program_, vector_add_name, &error);
    return steps_.check_result("kernel", error);
}

/* Each argument is a cl_mem, passed as the handle the host gave: the host
 * resolves it, never the guest. */
Status OpenClScenario::compute()
{
    AV_SCOPE("OpenClScenario::compute");
    KmtTransport &transport = link_.transport();
    cl_int        ran = CL_SUCCESS;
    for (cl_uint index = 0u; index < buffer_count && ran == CL_SUCCESS; index++)
        ran = clproto::av_clSetKernelArg(transport, kernel_, index, sizeof buffers_[index], &buffers_[index]);
    const size_t global = elements;
    if (ran == CL_SUCCESS)
        ran = clproto::av_clEnqueueNDRangeKernel(transport, queue_, kernel_, 1u, nullptr, &global, nullptr, 0u,
                                                  nullptr, nullptr);
    if (ran == CL_SUCCESS) ran = clproto::av_clFinish(transport, queue_);
    const Status judged = steps_.check_result("run", ran);
    if (judged != Status::ok) return judged;
    for (float &element : output_) element = 0.0f;
    const cl_int read = clproto::av_clEnqueueReadBuffer(transport, queue_, buffers_[output_index], CL_TRUE, 0u,
                                                         sizeof output_, output_, 0u, nullptr, nullptr);
    const Status readback = steps_.check_result("read", read);
    if (readback != Status::ok) return readback;
    uint32_t matched = 0u;
    for (uint32_t index = 0u; index < elements; index++)
        matched += output_[index] == inputs_[0][index] + inputs_[1][index] ? 1u : 0u;
    return steps_.check_measured("sum", matched == elements ? Status::ok : Status::conflict, matched);
}

/* An object never made is not released: a null handle is refused by the host
 * as undecodable, which would fault the context before the rest are. */
void OpenClScenario::release()
{
    AV_SCOPE("OpenClScenario::release");
    release_error_ = CL_SUCCESS;
    release_object(&clproto::av_clReleaseKernel, kernel_);
    release_object(&clproto::av_clReleaseProgram, program_);
    for (uint32_t index = buffer_count; index-- > 0u;) release_object(&clproto::av_clReleaseMemObject, buffers_[index]);
    release_object(&clproto::av_clReleaseCommandQueue, queue_);
    release_object(&clproto::av_clReleaseContext, context_);
    device_ = 0u;
}

void OpenClScenario::release_object(Release call, uint64_t &object)
{
    AV_SCOPE("OpenClScenario::release_object");
    if (object == 0u) return;
    const cl_int released = call(link_.transport(), object);
    object = 0u;
    if (release_error_ == CL_SUCCESS) release_error_ = released;
}

}  // namespace av
