#ifndef AV_GATHERED_COMMAND_HPP
#define AV_GATHERED_COMMAND_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class Host;
struct GatheredCommandProbe;

class GatheredCommand {
public:
    explicit GatheredCommand(Host &host);
    ~GatheredCommand();
    GatheredCommand(const GatheredCommand &) = delete;
    GatheredCommand &operator=(const GatheredCommand &) = delete;

    [[nodiscard]] Status gather(const uint8_t *body, size_t length);
    bool                 gathering() const;
    [[nodiscard]] Status close(const uint8_t *body, size_t length, const uint8_t *&payload, size_t &size);
    void                 clear();

    bool invariants_hold() const;

private:
    [[nodiscard]] Status append(const uint8_t *body, size_t length);

    Host    &host_;
    uint8_t *block_;
    size_t   capacity_;
    size_t   used_;

    friend struct GatheredCommandProbe;
};

}

#endif
