#include "device/control/window_stats_commands.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "device/control/control_request.hpp"
#include "device/host/host.hpp"
#include "device/memory/window_stats_window.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/page_request.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

WindowStatsCommands::WindowStatsCommands(Host &host, WindowStatsWindow &window) : host_(host), window_(window)
{
    AV_SCOPE("WindowStatsCommands::WindowStatsCommands");
}

bool WindowStatsCommands::answer(ControlRequest &request, uint32_t type)
{
    AV_SCOPE("WindowStatsCommands::answer");
    host_.require_main_loop("WindowStatsCommands::answer");
    if (type != PageRequest::window_stats) return false;
    register_page(request);
    return true;
}

void WindowStatsCommands::register_page(ControlRequest &request)
{
    AV_SCOPE("WindowStatsCommands::register_page");
    uint8_t      bytes[PageRequest::encoded_size] = {};
    WireReader   reader(bytes, request.read(0u, bytes, sizeof bytes));
    PageRequest  page;
    GpuResponse  response;
    const Status attached = page.decode(reader) == Status::ok ? window_.map(page.address()) : Status::malformed;
    response.set(attached == Status::ok ? GpuResponse::ok_nodata : GpuResponse::err_invalid_parameter);
    if (attached != Status::ok) {
        char       line[112] = {};
        TextBuffer text(line, sizeof line);
        text.append("virtio-gpu-av: the host window counters' page was refused, status ");
        text.append_decimal(static_cast<uint32_t>(attached));
        host_.log(text.text());
    }
    request.respond(response);
}

}
