/* A guest's blocking read served from one larger read made ahead of it, on the
 * host's own OpenCL. What any host can judge is every read it declines; a host
 * with an OpenCL GPU also shows the pieces after the first being copies of
 * what the first read, that a piece outside the copy reads again, and that a
 * forgotten copy is never served. A host without one returns after the
 * refusals, as the other device tests do. */
#include "executor/opencl/cl_read_ahead.hpp"
#include "harness/framework/test_registry.hpp"

#include <CL/cl.h>

#include <cstddef>
#include <cstdint>

using av::ClReadAhead;

namespace {

constexpr std::size_t buffer_bytes = 3u * 65536u;

/* A context, a queue and a buffer filled with each byte's index modulo 251 on
 * the first OpenCL GPU, or nothing. */
class HostBuffer {
public:
    HostBuffer() : context_(nullptr), queue_(nullptr), buffer_(nullptr)
    {
        cl_platform_id platforms[8] = {};
        cl_uint        count = 0u;
        if (clGetPlatformIDs(8u, platforms, &count) != CL_SUCCESS) return;
        for (cl_uint index = 0u; index < count && buffer_ == nullptr; index++) {
            cl_device_id device = nullptr;
            if (clGetDeviceIDs(platforms[index], CL_DEVICE_TYPE_GPU, 1u, &device, nullptr) != CL_SUCCESS) continue;
            cl_int made = CL_SUCCESS;
            context_ = clCreateContext(nullptr, 1u, &device, nullptr, nullptr, &made);
            if (context_ == nullptr) continue;
            queue_ = clCreateCommandQueueWithProperties(context_, device, nullptr, &made);
            static std::uint8_t pattern[buffer_bytes];
            for (std::size_t i = 0u; i < buffer_bytes; i++) pattern[i] = static_cast<std::uint8_t>(i % 251u);
            buffer_ = clCreateBuffer(context_, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, buffer_bytes, pattern, &made);
        }
    }
    ~HostBuffer()
    {
        if (buffer_ != nullptr) clReleaseMemObject(buffer_);
        if (queue_ != nullptr) clReleaseCommandQueue(queue_);
        if (context_ != nullptr) clReleaseContext(context_);
    }
    HostBuffer(const HostBuffer &) = delete;
    HostBuffer &operator=(const HostBuffer &) = delete;

    bool             made() const { return queue_ != nullptr && buffer_ != nullptr; }
    cl_command_queue queue() const { return queue_; }
    cl_mem           buffer() const { return buffer_; }
    /* Overwrites the buffer's first byte behind the read-ahead's back. */
    void             scribble(std::uint8_t value)
    {
        static_cast<void>(clEnqueueWriteBuffer(queue_, buffer_, CL_TRUE, 0u, 1u, &value, 0u, nullptr, nullptr));
    }

private:
    cl_context       context_;
    cl_command_queue queue_;
    cl_mem           buffer_;
};

bool holds_pattern(const std::uint8_t *bytes, std::size_t offset, std::size_t size)
{
    for (std::size_t i = 0u; i < size; i++)
        if (bytes[i] != static_cast<std::uint8_t>((offset + i) % 251u)) return false;
    return true;
}

}  // namespace

TEST(cl_read_ahead_read, declines_an_empty_or_oversized_read_and_one_with_nowhere_to_go)
{ ClReadAhead ahead;
  std::uint8_t into[16] = {};
  cl_int result = 7;
  CHECK(!ahead.read(nullptr, nullptr, 0u, 0u, into, result));
  CHECK(!ahead.read(nullptr, nullptr, 0u, ClReadAhead::cache_bytes + 1u, into, result));
  CHECK(!ahead.read(nullptr, nullptr, 0u, 16u, nullptr, result));
  /* A buffer that is not one: its size cannot be asked. */
  CHECK(!ahead.read(nullptr, nullptr, 0u, 16u, into, result));
  CHECK_EQ(result, 7);
  ahead.forget(); }

TEST(cl_read_ahead_read, serves_the_pieces_after_the_first_from_the_copy)
{ HostBuffer host;
  if (!host.made()) return;
  ClReadAhead ahead;
  std::uint8_t piece[4096] = {};
  cl_int result = 1;
  CHECK(ahead.read(host.queue(), host.buffer(), 1000u, sizeof piece, piece, result));
  CHECK_EQ(result, CL_SUCCESS);
  CHECK(holds_pattern(piece, 1000u, sizeof piece));
  /* A change the copy cannot see: the next piece is the copy's, as the device
   * guarantees no command ran between (OpenClBackend::run forgets first). */
  host.scribble(0xEEu);
  result = 1;
  CHECK(ahead.read(host.queue(), host.buffer(), 1000u + 4096u, sizeof piece, piece, result));
  CHECK_EQ(result, CL_SUCCESS);
  CHECK(holds_pattern(piece, 1000u + 4096u, sizeof piece));
  /* The last byte of the buffer is still inside what was read ahead. */
  std::uint8_t last = 0u;
  CHECK(ahead.read(host.queue(), host.buffer(), buffer_bytes - 1u, 1u, &last, result));
  CHECK_EQ(last, static_cast<std::uint8_t>((buffer_bytes - 1u) % 251u));
  /* Before the copy: read again, from the GPU, which now holds the scribble. */
  std::uint8_t first[8] = {};
  CHECK(ahead.read(host.queue(), host.buffer(), 0u, sizeof first, first, result));
  CHECK_EQ(first[0], 0xEEu);
  CHECK(holds_pattern(first + 1u, 1u, sizeof first - 1u)); }

TEST(cl_read_ahead_read, a_read_that_is_the_rest_of_the_buffer_or_past_it_is_the_caller_s)
{ HostBuffer host;
  if (!host.made()) return;
  ClReadAhead ahead;
  static std::uint8_t rest[buffer_bytes];
  cl_int result = 1;
  CHECK(!ahead.read(host.queue(), host.buffer(), 0u, buffer_bytes, rest, result));
  CHECK(!ahead.read(host.queue(), host.buffer(), buffer_bytes - 16u, 16u, rest, result));
  CHECK(!ahead.read(host.queue(), host.buffer(), buffer_bytes, 1u, rest, result));
  CHECK_EQ(result, 1); }

TEST(cl_read_ahead_forget, a_forgotten_copy_is_never_served)
{ HostBuffer host;
  if (!host.made()) return;
  ClReadAhead ahead;
  std::uint8_t piece[64] = {};
  cl_int result = 1;
  CHECK(ahead.read(host.queue(), host.buffer(), 0u, sizeof piece, piece, result));
  host.scribble(0x11u);
  ahead.forget();
  CHECK(ahead.read(host.queue(), host.buffer(), 0u, sizeof piece, piece, result));
  CHECK_EQ(piece[0], 0x11u);
  CHECK(holds_pattern(piece + 1u, 1u, sizeof piece - 1u)); }
