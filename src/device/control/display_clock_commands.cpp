#include "device/control/display_clock_commands.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "device/control/control_request.hpp"
#include "device/display/display_clock.hpp"
#include "device/host/host.hpp"
#include "proto/virtio_gpu/page_request.hpp"
#include "proto/virtio_gpu/display_timing_request.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

DisplayClockCommands::DisplayClockCommands(Host &host, DisplayClock &clock) : host_(host), clock_(clock)
{
    AV_SCOPE("DisplayClockCommands::DisplayClockCommands");
}

bool DisplayClockCommands::answer(ControlRequest &request, uint32_t type)
{
    AV_SCOPE("DisplayClockCommands::answer");
    host_.require_main_loop("DisplayClockCommands::answer");
    if (type == PageRequest::display_clock) register_page(request);
    else if (type == DisplayTimingRequest::type) retime(request);
    else return false;
    return true;
}

void DisplayClockCommands::register_page(ControlRequest &request)
{
    AV_SCOPE("DisplayClockCommands::register_page");
    uint8_t      bytes[PageRequest::encoded_size] = {};
    WireReader   reader(bytes, request.read(0u, bytes, sizeof bytes));
    PageRequest  clock;
    GpuResponse  response;
    const Status attached = clock.decode(reader) == Status::ok ? clock_.attach(clock.address()) : Status::malformed;
    if (attached == Status::no_memory) response.set(GpuResponse::err_out_of_memory);
    else response.set(attached == Status::ok ? GpuResponse::ok_nodata : GpuResponse::err_invalid_parameter);
    if (attached != Status::ok) {
        char       line[112] = {};
        TextBuffer text(line, sizeof line);
        text.append("virtio-gpu-av: the display clock's page was refused, status ");
        text.append_decimal(static_cast<uint32_t>(attached));
        host_.log(text.text());
    }
    request.respond(response);
}

void DisplayClockCommands::retime(ControlRequest &request)
{
    AV_SCOPE("DisplayClockCommands::retime");
    uint8_t              bytes[DisplayTimingRequest::encoded_size] = {};
    WireReader           reader(bytes, request.read(0u, bytes, sizeof bytes));
    DisplayTimingRequest timing;
    GpuResponse          response;
    const bool           retimed = timing.decode(reader) == Status::ok &&
                                   clock_.retime(timing.head(), timing.refresh_millihz()) == Status::ok;
    response.set(retimed ? GpuResponse::ok_nodata : GpuResponse::err_invalid_parameter);
    request.respond(response);
}

}
