#ifndef AV_CLICD_CL_PENDING_READ_HPP
#define AV_CLICD_CL_PENDING_READ_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

struct ClPendingReadProbe;

class ClPendingRead {
public:
    ClPendingRead();

    [[nodiscard]] bool take(uint64_t event, uint64_t queue, uint64_t buffer, size_t offset,
                            size_t bytes, void *into, bool ours, uint64_t scratch = 0u);
    void               give_up();
    bool               held() const;

    uint64_t event() const;
    uint64_t queue() const;
    uint64_t buffer() const;
    size_t   offset() const;
    size_t   bytes() const;
    void    *into() const;
    bool     ours() const;
    uint64_t scratch() const;

    bool invariants_hold() const;

private:
    friend struct ClPendingReadProbe;

    void    *into_;
    uint64_t event_;
    uint64_t queue_;
    uint64_t buffer_;
    uint64_t scratch_;
    size_t   offset_;
    size_t   bytes_;
    bool     ours_;
    bool     held_;
};

}

#endif
