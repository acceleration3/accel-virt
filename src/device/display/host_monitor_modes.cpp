#include "device/display/host_monitor_modes.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"

namespace av {

namespace {

constexpr const char *drm_class = "/sys/class/drm";
constexpr uint32_t    decimal_base = 10u;
constexpr uint32_t    max_digits = 6u;

/* One line of a connector's modes file, "2560x1440", as its two edges; false
 * for anything else - an interlaced "1920x1080i" among them. */
bool line_mode(const char *line, size_t length, uint32_t &width, uint32_t &height)
{
    AV_SCOPE("line_mode");
    uint32_t *edges[2] = {&width, &height};
    size_t    at = 0u;
    for (uint32_t side = 0u; side < 2u; side++) {
        *edges[side] = 0u;
        uint32_t digits = 0u;
        for (; at < length && line[at] >= '0' && line[at] <= '9'; at++) {
            if (++digits > max_digits) return false;
            *edges[side] = *edges[side] * decimal_base + static_cast<uint32_t>(line[at] - '0');
        }
        if (digits == 0u) return false;
        if (side == 0u) {
            if (at == length || line[at] != 'x') return false;
            at++;
        }
    }
    return at == length;
}

}  // namespace

HostMonitorModes::HostMonitorModes(Host &host) : host_(host)
{
    AV_SCOPE("HostMonitorModes::HostMonitorModes");
}

Status HostMonitorModes::read(DisplayModeSet &modes)
{
    AV_SCOPE("HostMonitorModes::read");
    host_.require_main_loop("HostMonitorModes::read");
    char   listing[listing_bytes];
    size_t listed = 0u;
    if (host_.list_directory(drm_class, listing, sizeof listing, listed) != Status::ok) return Status::not_found;
    DisplayModeSet found;
    uint64_t       best_area = 0u;
    uint32_t       best_width = 0u;
    uint32_t       best_height = 0u;
    for (size_t start = 0u; start < listed;) {
        size_t end = start;
        while (end < listed && listing[end] != '\n') end++;
        listing[end] = '\0';
        const char *const name = &listing[start];
        start = end + 1u;
        /* A connector is its card's name, a dash and the port: card1-DP-2. */
        bool connector = name[0] == 'c';
        for (const char *at = name; *at != '\0' && connector; at++)
            if (*at == '-') break;
            else if (at[1] == '\0') connector = false;
        if (!connector || !connected(name)) continue;
        uint32_t width = 0u;
        uint32_t height = 0u;
        add_modes(name, found, width, height);
        if (uint64_t{width} * height > best_area) {
            best_area = uint64_t{width} * height;
            best_width = width;
            best_height = height;
        }
    }
    if (found.count() == 0u) return Status::not_found;
    for (uint32_t index = 0u; index < found.count(); index++)
        if (found.width(index) == best_width && found.height(index) == best_height)
            static_cast<void>(found.prefer(index));
    modes = found;
    return Status::ok;
}

bool HostMonitorModes::connected(const char *name)
{
    AV_SCOPE("HostMonitorModes::connected");
    char       path_text[path_bytes];
    TextBuffer path(path_text, sizeof path_text);
    path.append(drm_class);
    path.append("/");
    path.append(name);
    path.append("/status");
    char        status[32];
    size_t      length = 0u;
    const char  word[] = "connected";
    if (host_.read_file(path.text(), status, sizeof status, length) != Status::ok) return false;
    /* "disconnected" ends the same way, so the whole word from the start. */
    for (size_t at = 0u; at + 1u < sizeof word; at++)
        if (at >= length || status[at] != word[at]) return false;
    return length == sizeof word - 1u || status[sizeof word - 1u] == '\n';
}

void HostMonitorModes::add_modes(const char *name, DisplayModeSet &modes, uint32_t &best_width, uint32_t &best_height)
{
    AV_SCOPE("HostMonitorModes::add_modes");
    best_width = 0u;
    best_height = 0u;
    char       path_text[path_bytes];
    TextBuffer path(path_text, sizeof path_text);
    path.append(drm_class);
    path.append("/");
    path.append(name);
    path.append("/modes");
    char   text[file_bytes];
    size_t length = 0u;
    if (host_.read_file(path.text(), text, sizeof text, length) != Status::ok) return;
    for (size_t start = 0u; start < length;) {
        size_t end = start;
        while (end < length && text[end] != '\n') end++;
        uint32_t width = 0u;
        uint32_t height = 0u;
        const bool usable = line_mode(&text[start], end - start, width, height) && width >= least_width &&
                            height >= least_height;
        start = end + 1u;
        if (!usable) continue;
        /* The first usable line is the monitor's best: its kernel lists the
         * preferred mode first. */
        if (best_width == 0u) {
            best_width = width;
            best_height = height;
        }
        if (modes.add(width, height) == Status::no_space) return;
    }
}

}  // namespace av
