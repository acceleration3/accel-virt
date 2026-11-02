#include "core/support/status.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

std::uint64_t unsigned_argument(char **argv, int index) { return std::strtoull(argv[index], nullptr, 0); }
std::uint32_t u32_argument(char **argv, int index) { return static_cast<std::uint32_t>(unsigned_argument(argv, index)); }
std::int32_t  i32_argument(char **argv, int index) { return static_cast<std::int32_t>(std::strtol(argv[index], nullptr, 0)); }

}

int main(int argc, char **argv)
{
    std::uint8_t bytes[av::GpuRequestWriter::max_request_size] = {};
    av::GpuRequestWriter writer(bytes, sizeof bytes);
    const char *const request = argc > 1 ? argv[1] : "";
    av::Status status = av::Status::invalid_parameter;
    if (std::strcmp(request, "create_2d") == 0 && argc == 6)
        status = writer.create_2d(u32_argument(argv, 2), u32_argument(argv, 3), u32_argument(argv, 4),
                                  u32_argument(argv, 5));
    if (std::strcmp(request, "attach_backing") == 0 && argc == 5)
        status = writer.attach_backing(u32_argument(argv, 2), unsigned_argument(argv, 3), u32_argument(argv, 4));
    if (std::strcmp(request, "transfer_to_host_2d") == 0 && argc == 5)
        status = writer.transfer_to_host_2d(u32_argument(argv, 2), u32_argument(argv, 3), u32_argument(argv, 4));
    if (std::strcmp(request, "update_cursor") == 0 && argc == 8)
        status = writer.update_cursor(u32_argument(argv, 2), i32_argument(argv, 3), i32_argument(argv, 4),
                                      u32_argument(argv, 5), u32_argument(argv, 6), u32_argument(argv, 7));
    if (std::strcmp(request, "move_cursor") == 0 && argc == 6)
        status = writer.move_cursor(u32_argument(argv, 2), i32_argument(argv, 3), i32_argument(argv, 4),
                                    u32_argument(argv, 5));
    if (status != av::Status::ok) {
        std::fprintf(stderr, "gpu_request_bytes: cannot write %s with %d argument(s)\n", request, argc - 2);
        return 2;
    }
    for (std::size_t index = 0u; index < writer.length(); index++) std::printf("%02x", bytes[index]);
    std::printf("\n");
    return 0;
}
