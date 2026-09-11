#include "display_mode_list.hpp"

namespace av {

DisplayModeList::DisplayModeList()
    : modes_{DisplayMode(640u, 480u),   DisplayMode(800u, 600u),   DisplayMode(1024u, 768u),
             DisplayMode(1280u, 720u),  DisplayMode(1280u, 800u),  DisplayMode(1366u, 768u),
             DisplayMode(1440u, 900u),  DisplayMode(1280u, 1024u), DisplayMode(1600u, 900u),
             DisplayMode(1680u, 1050u), DisplayMode(1920u, 1080u)} {}

uint32_t DisplayModeList::count() const { return mode_count; }

const DisplayMode &DisplayModeList::mode(uint32_t index) const
{
    return modes_[index < mode_count ? index : 0u];
}

const DisplayMode &DisplayModeList::preferred() const { return modes_[preferred_index]; }

bool DisplayModeList::is_preferred(const DisplayMode &mode) const { return &mode == &modes_[preferred_index]; }

const DisplayMode *DisplayModeList::find(const D3dkmdtVidPnSourceModeRecord &source) const
{
    for (const DisplayMode &mode : modes_)
        if (mode.matches(source)) return &mode;
    return nullptr;
}

const DisplayMode *DisplayModeList::find(const D3dkmdtVideoSignalInfoRecord &signal) const
{
    for (const DisplayMode &mode : modes_)
        if (mode.matches(signal)) return &mode;
    return nullptr;
}

}  // namespace av
