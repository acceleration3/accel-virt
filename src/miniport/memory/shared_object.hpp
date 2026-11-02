#ifndef AV_SHARED_OBJECT_HPP
#define AV_SHARED_OBJECT_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/resources/shared_object_kind.hpp"

#include <stdint.h>

namespace av {

class Allocation;
struct SharedObjectProbe;

class SharedObject {
public:
    SharedObject(Platform &platform, uint64_t id, const SharedObjectKind &kind, uint64_t size,
                 Allocation &allocation);
    SharedObject(const SharedObject &) = delete;
    SharedObject &operator=(const SharedObject &) = delete;

    uint64_t                id() const;
    const SharedObjectKind &kind() const;
    uint64_t                size() const;
    bool                    names(const Allocation &allocation) const;
    void                    forget();
    bool                    forgotten() const;
    uint32_t                opens() const;

    [[nodiscard]] Status open();
    [[nodiscard]] Status close();

    bool invariants_hold() const;

private:
    static constexpr uint32_t max_opens = 0xFFFFFFFFu;

    Platform        &platform_;
    uint64_t         id_;
    SharedObjectKind kind_;
    uint64_t         size_;
    Allocation      *allocation_;
    uint32_t         opens_;

    friend struct SharedObjectProbe;
};

}

#endif
