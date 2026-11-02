/* PRESENT_ENGINE and QUEUED_PRESENT (docs/protocol.md, "Present engine"):
 * where the device says which queued presents it has moved, and the presents
 * themselves, handed to the engine. Main loop. */
#ifndef AV_PRESENT_ENGINE_COMMANDS_HPP
#define AV_PRESENT_ENGINE_COMMANDS_HPP

#include <stdint.h>

namespace av {

class ControlRequest;
class GuestRamLendings;
class Host;
class PresentEngine;
class PresentFrames;

class PresentEngineCommands {
public:
    /* The engine reads image sources from `snapshots`, from the page on, and
     * has the GPU copy them into the memory of `lendings`. */
    PresentEngineCommands(Host &host, PresentEngine &engine, const PresentFrames &snapshots,
                          GuestRamLendings &lendings);
    PresentEngineCommands(const PresentEngineCommands &) = delete;
    PresentEngineCommands &operator=(const PresentEngineCommands &) = delete;

    /* Whether the core answered `request`; false leaves it to the base class. */
    bool answer(ControlRequest &request, uint32_t type);

private:
    void register_page(ControlRequest &request);

    Host                   &host_;
    PresentEngine          &engine_;
    const PresentFrames &snapshots_;
    GuestRamLendings &lendings_;
};

}  // namespace av

#endif  // AV_PRESENT_ENGINE_COMMANDS_HPP
