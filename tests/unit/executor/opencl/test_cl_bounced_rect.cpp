/* A rectangle read into private memory and copied into the arena a row at a
 * time, on the host's own OpenCL. Any host judges what counts as a refusal and
 * every region the read itself refuses; a host with an OpenCL GPU also shows
 * the rows landing where the host would have put them, the gaps between them
 * left as they were, the private memory growing for a larger read, and the
 * host's own error passed back. A host without one returns after the
 * refusals, as the other device tests do. */
#include "executor/opencl/cl_bounced_rect.hpp"
#include "harness/framework/test_registry.hpp"

#include <CL/cl.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ClBouncedRect;

namespace {

constexpr std::size_t buffer_bytes = 256u * 1024u;

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

private:
    cl_context       context_;
    cl_command_queue queue_;
    cl_mem           buffer_;
};

/* Whether `into`, read at host origin `host` and the given pitches from the
 * buffer at origin `from` and its pitches, holds the pattern in every row of
 * `region` and 0xEE everywhere else among its first `bytes`. The rows are
 * judged, then struck out of a copy, which must then be 0xEE throughout: the
 * pattern holds 0xEE too. */
bool rows_landed(const std::uint8_t *into, std::size_t bytes, const std::size_t *from, std::size_t buffer_row,
                 std::size_t buffer_slice, const std::size_t *host, const std::size_t *region, std::size_t row,
                 std::size_t slice)
{
    static std::uint8_t rest[buffer_bytes];
    if (bytes > sizeof rest) return false;
    std::memcpy(rest, into, bytes);
    for (std::size_t z = 0u; z < region[2]; z++)
        for (std::size_t y = 0u; y < region[1]; y++)
            for (std::size_t x = 0u; x < region[0]; x++) {
                const std::size_t at = (host[2] + z) * slice + (host[1] + y) * row + host[0] + x;
                const std::size_t source = (from[2] + z) * buffer_slice + (from[1] + y) * buffer_row + from[0] + x;
                if (into[at] != static_cast<std::uint8_t>(source % 251u)) return false;
                rest[at] = 0xEEu;
            }
    for (std::size_t i = 0u; i < bytes; i++)
        if (rest[i] != 0xEEu) return false;
    return true;
}

}  // namespace

TEST(cl_bounced_rect_refusal, only_a_destination_the_host_will_not_take_is_one_and_it_is_kept)
{ ClBouncedRect bounced;
  CHECK(!bounced.bounces());
  CHECK(!bounced.refusal(CL_SUCCESS));
  CHECK(!bounced.refusal(CL_INVALID_VALUE));
  CHECK(!bounced.refusal(CL_INVALID_MEM_OBJECT));
  CHECK(!bounced.bounces());
  CHECK(bounced.refusal(CL_OUT_OF_RESOURCES));
  CHECK(bounced.bounces());
  CHECK(!bounced.refusal(CL_SUCCESS));
  CHECK(bounced.bounces());
  ClBouncedRect host_memory;
  CHECK(host_memory.refusal(CL_OUT_OF_HOST_MEMORY));
  CHECK(host_memory.bounces());
  ClBouncedRect allocation;
  CHECK(allocation.refusal(CL_MEM_OBJECT_ALLOCATION_FAILURE));
  CHECK(allocation.bounces()); }

TEST(cl_bounced_rect_read, refuses_a_region_that_names_no_rows_or_reaches_past_memory)
{ ClBouncedRect bounced;
  std::uint8_t into[64] = {};
  const std::size_t origin[3] = {0u, 0u, 0u}, region[3] = {4u, 2u, 1u};
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, nullptr, region, 0u, 0u, 0u, 0u, into, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, origin, nullptr, 0u, 0u, 0u, 0u, into, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, origin, region, 0u, 0u, 0u, 0u, nullptr, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  const std::size_t flat[3][3] = {{0u, 2u, 1u}, {4u, 0u, 1u}, {4u, 2u, 0u}};
  for (const auto &empty : flat)
      CHECK_EQ(bounced.read(nullptr, nullptr, origin, origin, empty, 0u, 0u, 0u, 0u, into, 0u, nullptr, nullptr),
               CL_INVALID_VALUE);
  /* A row shorter than the region's width, and a slice shorter than its rows. */
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, origin, region, 0u, 0u, 3u, 0u, into, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, origin, region, 0u, 0u, 4u, 7u, into, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  /* Pitches and origins whose reach wraps, at each step of it. */
  const std::size_t most = SIZE_MAX;
  const std::size_t wide[3] = {most, 2u, 1u};
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, origin, wide, 0u, 0u, 0u, 0u, into, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  const std::size_t far_row[3] = {0u, most, 0u}, far_slice[3] = {0u, 0u, most}, far_column[3] = {most, 1u, 0u};
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, far_row, region, 0u, 0u, 0u, 0u, into, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, far_slice, region, 0u, 0u, 0u, 0u, into, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, far_column, region, 0u, 0u, 0u, 0u, into, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  const std::size_t both[3] = {0u, most / 4u, most / 8u};
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, both, region, 0u, 0u, 0u, 0u, into, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  const std::size_t deep[3] = {4u, 2u, most};
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, origin, deep, 0u, 0u, 0u, 0u, into, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  /* The last slice, the last row and the last row's width each past the end. */
  const std::size_t two_slices[3] = {4u, 1u, 2u}, near_slice[3] = {most - 10u, 0u, 0u};
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, near_slice, two_slices, 0u, 0u, 4u, 16u, into, 0u, nullptr,
                        nullptr),
           CL_INVALID_VALUE);
  const std::size_t near_row[3] = {most - 5u, 0u, 0u};
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, near_row, region, 0u, 0u, 8u, 0u, into, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  const std::size_t edge[3] = {most - 7u, 0u, 0u};
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, edge, region, 0u, 0u, 0u, 0u, into, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  /* A reach that does not wrap, but that no private memory can hold. */
  const std::size_t near_end[3] = {most - 20u, 0u, 0u}, one[3] = {4u, 1u, 1u};
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, near_end, one, 0u, 0u, 0u, 0u, into, 0u, nullptr, nullptr),
           CL_OUT_OF_HOST_MEMORY);
  const std::size_t exabyte[3] = {std::size_t{1} << 60u, 0u, 0u};
  CHECK_EQ(bounced.read(nullptr, nullptr, origin, exabyte, one, 0u, 0u, 0u, 0u, into, 0u, nullptr, nullptr),
           CL_OUT_OF_HOST_MEMORY);
  /* Nothing was written. */
  std::uint8_t zero[64] = {};
  CHECK(std::memcmp(into, zero, sizeof into) == 0); }

TEST(cl_bounced_rect_read, lands_every_row_where_the_host_would_and_leaves_the_gaps)
{ HostBuffer host;
  if (!host.made()) return;
  ClBouncedRect bounced;
  static std::uint8_t into[64u * 1024u];
  std::memset(into, 0xEE, sizeof into);
  const std::size_t from[3] = {16u, 3u, 1u}, at[3] = {8u, 2u, 1u}, region[3] = {100u, 5u, 2u};
  CHECK_EQ(bounced.read(host.queue(), host.buffer(), from, at, region, 256u, 256u * 40u, 300u, 300u * 45u, into,
                        0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK(rows_landed(into, sizeof into, from, 256u, 256u * 40u, at, region, 300u, 300u * 45u));
  /* Pitches left to the host: rows packed, slices of packed rows. */
  std::memset(into, 0xEE, sizeof into);
  const std::size_t corner[3] = {0u, 0u, 0u}, packed[3] = {64u, 4u, 3u};
  CHECK_EQ(bounced.read(host.queue(), host.buffer(), from, corner, packed, 512u, 512u * 8u, 0u, 0u, into, 0u,
                        nullptr, nullptr),
           CL_SUCCESS);
  CHECK(rows_landed(into, sizeof into, from, 512u, 512u * 8u, corner, packed, 64u, 64u * 4u));
  /* A read larger than the private memory held so far grows it, and an event
   * asked for is the host's. */
  static std::uint8_t large[buffer_bytes];
  std::memset(large, 0xEE, sizeof large);
  const std::size_t whole[3] = {1024u, 200u, 1u};
  cl_event read = nullptr;
  CHECK_EQ(bounced.read(host.queue(), host.buffer(), corner, corner, whole, 1024u, 0u, 1024u, 0u, large, 0u,
                        nullptr, &read),
           CL_SUCCESS);
  CHECK(read != nullptr);
  clReleaseEvent(read);
  CHECK(rows_landed(large, sizeof large, corner, 1024u, 1024u * 200u, corner, whole, 1024u, 1024u * 200u)); }

TEST(cl_bounced_rect_read, the_host_s_own_refusal_is_answered_and_nothing_is_copied)
{ HostBuffer host;
  if (!host.made()) return;
  ClBouncedRect bounced;
  std::uint8_t into[256];
  std::memset(into, 0xEE, sizeof into);
  /* A rectangle past the buffer's end is the host's to refuse. */
  const std::size_t beyond[3] = {0u, buffer_bytes / 64u, 0u}, corner[3] = {0u, 0u, 0u}, region[3] = {64u, 2u, 1u};
  CHECK_EQ(bounced.read(host.queue(), host.buffer(), beyond, corner, region, 64u, 0u, 0u, 0u, into, 0u, nullptr,
                        nullptr),
           CL_INVALID_VALUE);
  std::size_t untouched = 0u;
  for (const std::uint8_t byte : into) untouched += byte == 0xEEu ? 1u : 0u;
  CHECK_EQ(untouched, sizeof into); }
