#include "core/display/display_mode_list.hpp"

#include "core/display/display_info_response.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

DisplayModeList::DisplayModeList()
    : standard_{DisplayMode(640u, 480u),   DisplayMode(800u, 600u),   DisplayMode(1024u, 768u),
                DisplayMode(1280u, 720u),  DisplayMode(1280u, 800u),  DisplayMode(1366u, 768u),
                DisplayMode(1440u, 900u),  DisplayMode(1280u, 1024u), DisplayMode(1600u, 900u),
                DisplayMode(1680u, 1050u), DisplayMode(1920u, 1080u)},
      modes_{DisplayMode(640u, 480u),   DisplayMode(800u, 600u),   DisplayMode(1024u, 768u),
             DisplayMode(1280u, 720u),  DisplayMode(1280u, 800u),  DisplayMode(1366u, 768u),
             DisplayMode(1440u, 900u),  DisplayMode(1280u, 1024u), DisplayMode(1600u, 900u),
             DisplayMode(1680u, 1050u), DisplayMode(1920u, 1080u), DisplayMode(1920u, 1080u)},
      count_(standard_count), preferred_index_(default_preferred_index)
      {
          AV_SCOPE("DisplayModeList::DisplayModeList");
      }

uint64_t DisplayModeList::area_of(const DisplayMode &mode) const
{
    AV_SCOPE("DisplayModeList::area_of");
    return uint64_t{mode.width()} * mode.height();
}

Status DisplayModeList::follow(const DisplayInfoResponse &info, uint32_t scanout)
{
    AV_SCOPE("DisplayModeList::follow");
    if (!info.enabled(scanout)) return Status::not_found;
    const DisplayMode display(info.width(scanout), info.height(scanout));
    uint32_t          count = 0u;
    uint32_t          preferred = max_modes;
    for (const DisplayMode &mode : standard_) {
        if (mode.width() > display.width() || mode.height() > display.height()) continue;
        if (mode.width() == display.width() && mode.height() == display.height()) preferred = count;
        modes_[count++] = mode;
    }
    /* A mode that fits has no more pixels than the display, so the display's
     * own size goes last and the list stays in ascending area - and it is
     * offered once, however standard it is. */
    if (preferred == max_modes) {
        preferred = count;
        modes_[count++] = display;
    }
    count_ = count;
    preferred_index_ = preferred;
    return Status::ok;
}

uint32_t DisplayModeList::count() const
{
    AV_SCOPE("DisplayModeList::count");
    return count_;
}

const DisplayMode &DisplayModeList::mode(uint32_t index) const
{
    AV_SCOPE("DisplayModeList::mode");
    return modes_[index < count_ ? index : 0u];
}

const DisplayMode &DisplayModeList::preferred() const
{
    AV_SCOPE("DisplayModeList::preferred");
    return modes_[preferred_index_];
}

bool DisplayModeList::is_preferred(const DisplayMode &mode) const
{
    AV_SCOPE("DisplayModeList::is_preferred");
    return &mode == &modes_[preferred_index_];
}

const DisplayMode *DisplayModeList::find(const D3dkmdtVidPnSourceModeRecord &source) const
{
    AV_SCOPE("DisplayModeList::find");
    for (uint32_t index = 0u; index < count_; index++)
        if (modes_[index].matches(source)) return &modes_[index];
    return nullptr;
}

const DisplayMode *DisplayModeList::find(const D3dkmdtVideoSignalInfoRecord &signal) const
{
    AV_SCOPE("DisplayModeList::find");
    for (uint32_t index = 0u; index < count_; index++)
        if (modes_[index].matches(signal)) return &modes_[index];
    return nullptr;
}

bool DisplayModeList::invariants_hold() const
{
    AV_SCOPE("DisplayModeList::invariants_hold");
    if (count_ == 0u || count_ > max_modes || preferred_index_ >= count_) return false;
    for (uint32_t index = 1u; index < count_; index++)
        if (area_of(modes_[index]) < area_of(modes_[index - 1u])) return false;
    return true;
}

}  // namespace av
