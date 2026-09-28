/* The driver's present copy (PresentCopyRequest): the pixels of a blt or a
 * colour fill moved between two surfaces' guest pages by the host, through its
 * own mapping of guest RAM, where the miniport's CPU sees them uncached.
 * Nothing moves unless the fixed part and both page lists hold; a region that
 * does not is refused where it goes wrong, the regions before it moved. The
 * answer comes only once every pixel has. Main loop. */
#ifndef AV_PRESENT_COMMANDS_HPP
#define AV_PRESENT_COMMANDS_HPP

#include <stdint.h>

namespace av {

class ControlRequest;
class Host;
class PresentCopyRequest;

class PresentCommands {
public:
    explicit PresentCommands(Host &host);
    PresentCommands(const PresentCommands &) = delete;
    PresentCommands &operator=(const PresentCommands &) = delete;

    /* Whether the core answered `request`; false leaves it to the base class. */
    bool answer(ControlRequest &request, uint32_t type);

private:
    /* The response type for a request whose fixed part decoded. */
    uint32_t move(ControlRequest &request, const PresentCopyRequest &copy);

    Host &host_;
};

}  // namespace av

#endif  // AV_PRESENT_COMMANDS_HPP
