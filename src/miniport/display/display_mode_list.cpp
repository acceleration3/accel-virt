#include "miniport/display/display_mode_list.hpp"

#include "miniport/display/display_info_response.hpp"
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
    if (preferred == max_modes) {
        preferred = count;
        modes_[count++] = display;
    }
    count_ = count;
    preferred_index_ = preferred;
    return Status::ok;
}

void DisplayModeList::insert(DisplayMode *taken, uint32_t &count, uint32_t &preferred, const DisplayMode &mode, bool chosen) const
{
    AV_SCOPE("DisplayModeList::insert");
    for (uint32_t index = 0u; index < count; index++) {
        const DisplayMode &held = taken[index];
        if (held.width() != mode.width() || held.height() != mode.height()) continue;
        const uint32_t apart = held.refresh_millihz() > mode.refresh_millihz() ? held.refresh_millihz() - mode.refresh_millihz()
                                                                                : mode.refresh_millihz() - held.refresh_millihz();
        if (apart < refresh_tolerance_millihz) {
            if (chosen) preferred = index;
            return;
        }
    }
    if (count == list_capacity) return;
    uint32_t at = count;
    while (at != 0u && (area_of(taken[at - 1u]) > area_of(mode) ||
                        (area_of(taken[at - 1u]) == area_of(mode) && taken[at - 1u].refresh_millihz() > mode.refresh_millihz()))) {
        taken[at] = taken[at - 1u];
        at--;
    }
    taken[at] = mode;
    if (preferred != list_capacity && preferred >= at) preferred++;
    if (chosen) preferred = at;
    count++;
}

Status DisplayModeList::adopt(const DisplayModeSet &set)
{
    AV_SCOPE("DisplayModeList::adopt");
    DisplayMode taken[list_capacity];
    uint32_t    count = 0u;
    uint32_t    preferred = list_capacity;
    for (uint32_t index = 0u; index < set.count(); index++) {
        const DisplayMode mode(set.width(index), set.height(index), set.refresh_millihz());
        if (!mode.offerable()) continue;
        insert(taken, count, preferred, mode, index == set.preferred());
    }
    if (count == 0u) return Status::not_found;
    const DisplayMode largest = taken[preferred == list_capacity ? count - 1u : preferred];
    for (const DisplayMode &standard : standard_)
        if (standard.width() <= largest.width() && standard.height() <= largest.height())
            insert(taken, count, preferred, DisplayMode(standard.width(), standard.height(), set.refresh_millihz()), false);
    const uint32_t hosted = count;
    DisplayMode    hosts[list_capacity];
    for (uint32_t index = 0u; index < hosted; index++) hosts[index] = taken[index];
    for (uint32_t index = 0u; index < hosted; index++)
        for (const uint32_t rate : offered_rates)
            insert(taken, count, preferred, DisplayMode(hosts[index].width(), hosts[index].height(), rate), false);
    if (preferred == list_capacity) preferred = count - 1u;
    for (uint32_t index = 0u; index < count; index++) modes_[index] = taken[index];
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
    const DisplayMode *best = nullptr;
    uint32_t           best_apart = 0u;
    const uint32_t     wanted = modes_[preferred_index_].refresh_millihz();
    for (uint32_t index = 0u; index < count_; index++) {
        if (!modes_[index].matches(source)) continue;
        const uint32_t rate = modes_[index].refresh_millihz();
        const uint32_t apart = rate > wanted ? rate - wanted : wanted - rate;
        if (best == nullptr || apart < best_apart) {
            best = &modes_[index];
            best_apart = apart;
        }
    }
    return best;
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

}
