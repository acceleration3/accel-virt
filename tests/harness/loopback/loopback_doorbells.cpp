#include "harness/loopback/loopback_doorbells.hpp"

#include "harness/host/mock_host.hpp"

namespace av {

LoopbackDoorbells::LoopbackDoorbells(MockHost &host) : host_(host), stores_(0u) {}

std::uint64_t LoopbackDoorbells::length() const { return pages * page_bytes; }
std::uint8_t  LoopbackDoorbells::read_u8(std::uint64_t) { return 0u; }
std::uint16_t LoopbackDoorbells::read_u16(std::uint64_t) { return 0u; }
std::uint32_t LoopbackDoorbells::read_u32(std::uint64_t) { return 0u; }
void          LoopbackDoorbells::write_u8(std::uint64_t, std::uint8_t) { stores_++; }
void          LoopbackDoorbells::write_u16(std::uint64_t, std::uint16_t) { stores_++; }
void          LoopbackDoorbells::write_u64(std::uint64_t, std::uint64_t) { stores_++; }

void LoopbackDoorbells::write_u32(std::uint64_t offset, std::uint32_t)
{
    stores_++;
    if (offset % page_bytes == 0u) static_cast<void>(host_.ring_doorbell(static_cast<std::uint32_t>(offset / page_bytes)));
}

std::uint32_t LoopbackDoorbells::stores() const { return stores_; }

}
