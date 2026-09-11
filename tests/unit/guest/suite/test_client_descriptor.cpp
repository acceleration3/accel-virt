#include "core/support/status.hpp"
#include "guest/suite/client_descriptor.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"

using av::AllocationDescriptor;
using av::AllocationKind;
using av::ClientDescriptor;
using av::Status;

TEST(client_descriptor, encodes_a_cpu_visible_client_allocation_rounded_to_pages)
{ const ClientDescriptor descriptor(4097u);
  CHECK_EQ(descriptor.size(), AllocationDescriptor::encoded_size);
  CHECK_EQ(descriptor.pages(), 2u);
  AllocationDescriptor decoded;
  CHECK_STATUS(decoded.decode(descriptor.bytes(), descriptor.size()), Status::ok);
  CHECK(decoded.kind() == AllocationKind::client);
  CHECK(decoded.cpu_visible());
  CHECK_EQ(decoded.size(), 4097u);
  CHECK_EQ(ClientDescriptor(4096u).pages(), 1u); }

TEST(client_descriptor, a_corrupted_magic_no_longer_decodes)
{ ClientDescriptor descriptor(4096u);
  descriptor.corrupt_magic();
  AllocationDescriptor decoded;
  CHECK_STATUS(decoded.decode(descriptor.bytes(), descriptor.size()), Status::malformed); }
