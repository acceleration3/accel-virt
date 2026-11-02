#include "executor/opencl/cl_binary_envelope.hpp"
#include "harness/framework/test_registry.hpp"

#include <CL/cl.h>

#include <cstdint>
#include <cstring>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:libRusticlOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libnvidia\n"
           "leak:libgallium\n"
           "leak:libLLVM\n"
           "leak:libdbus-1\n"
           "leak:libz\n";
}

namespace av {
struct ClBinaryEnvelopeProbe {
    static void another_device(ClBinaryEnvelope &envelope) { envelope.device_uuid_[0] ^= 0x01u; }
};

}

using av::ClBinaryEnvelope;

namespace {
bool first_gpu(cl_platform_id &platform, cl_device_id &device)
{
    cl_platform_id platforms[8] = {};
    cl_uint        found = 0u;
    if (::clGetPlatformIDs(8u, platforms, &found) != CL_SUCCESS) return false;
    for (cl_uint index = 0u; index < found && index < 8u; ++index) {
        cl_uint devices = 0u;
        if (::clGetDeviceIDs(platforms[index], CL_DEVICE_TYPE_GPU, 1u, &device, &devices) != CL_SUCCESS ||
            devices == 0u)
            continue;
        platform = platforms[index];
        return true;
    }
    return false;
}

constexpr std::uint8_t host_binary[] = {'a', 'n', 'y', ' ', 'h', 'o', 's', 't', ' ', 'b', 'y', 't', 'e', 's'};

}

TEST(cl_binary_envelope, an_envelope_that_knows_no_device_seals_and_opens_nothing)
{ ClBinaryEnvelope envelope;
  CHECK(!envelope.identified());
  CHECK(!envelope.identify(nullptr, nullptr));
  CHECK(!envelope.identified());
  CHECK_EQ(envelope.sealed_bytes(0u), 0u);
  CHECK_EQ(envelope.sealed_bytes(14u), ClBinaryEnvelope::header_bytes + 14u);
  std::uint8_t sealed[ClBinaryEnvelope::header_bytes + sizeof host_binary] = {};
  envelope.seal(host_binary, sizeof host_binary, sealed);
  const std::uint8_t *opened = nullptr;
  std::size_t         bytes = 0u;
  CHECK(!envelope.open(sealed, sizeof sealed, opened, bytes));
  CHECK(opened == nullptr); }

TEST(cl_binary_envelope, a_sealed_binary_opens_to_its_bytes_and_a_damaged_one_does_not)
{ cl_platform_id platform = nullptr;
  cl_device_id   device = nullptr;
  if (!first_gpu(platform, device)) return;
  ClBinaryEnvelope envelope;
  CHECK(envelope.identify(platform, device));
  CHECK(envelope.identified());
  std::uint8_t sealed[ClBinaryEnvelope::header_bytes + sizeof host_binary] = {};
  CHECK_EQ(envelope.sealed_bytes(sizeof host_binary), sizeof sealed);
  envelope.seal(host_binary, sizeof host_binary, sealed);
  CHECK(std::memcmp(sealed, "avgpu-cl", 8u) == 0);
  CHECK(std::memcmp(sealed + ClBinaryEnvelope::header_bytes, host_binary, sizeof host_binary) == 0);
  const std::uint8_t *opened = nullptr;
  std::size_t         bytes = 0u;
  CHECK(envelope.open(sealed, sizeof sealed, opened, bytes));
  CHECK(opened == sealed + ClBinaryEnvelope::header_bytes);
  CHECK_EQ(bytes, sizeof host_binary);

  for (std::size_t at = 0u; at < sizeof sealed; ++at) {
      sealed[at] ^= 0x10u;
      const std::uint8_t *damaged = nullptr;
      std::size_t         length = 0u;
      CHECK(!envelope.open(sealed, sizeof sealed, damaged, length));
      CHECK(damaged == nullptr);
      sealed[at] ^= 0x10u;
  }
  CHECK(!envelope.open(sealed, sizeof sealed - 1u, opened, bytes));
  std::uint8_t longer[sizeof sealed + 1u] = {};
  std::memcpy(longer, sealed, sizeof sealed);
  CHECK(!envelope.open(longer, sizeof longer, opened, bytes));
  CHECK(!envelope.open(sealed, ClBinaryEnvelope::header_bytes, opened, bytes));
  CHECK(!envelope.open(nullptr, sizeof sealed, opened, bytes));

  ClBinaryEnvelope other;
  CHECK(other.identify(platform, device));
  CHECK(other.open(sealed, sizeof sealed, opened, bytes));
  av::ClBinaryEnvelopeProbe::another_device(other);
  CHECK(!other.open(sealed, sizeof sealed, opened, bytes));
  std::uint8_t untouched[4] = {1u, 2u, 3u, 4u};
  envelope.seal(nullptr, 4u, untouched);
  envelope.seal(host_binary, 0u, untouched);
  CHECK_EQ(untouched[0], 1u); }
