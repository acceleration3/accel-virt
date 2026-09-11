/* Holds one object whose lifetime is started and ended explicitly - the
 * miniport root in the kernel shim's static storage, and the adapters inside
 * the root. The kernel runs no global constructors and there is no heap new, so
 * this is the one place in the codebase a placement-new expression appears
 * (tools/check_design.py enforces it).
 *
 * The destructor is deliberately trivial: storage with static lifetime must not
 * need a destructor call at unload. Whoever constructs destroys. */
#ifndef AV_OBJECT_STORAGE_HPP
#define AV_OBJECT_STORAGE_HPP

#include "core/support/status.hpp"

#include <new>

namespace av {

template <typename T>
class ObjectStorage {
public:
    ObjectStorage() = default;
    ObjectStorage(const ObjectStorage &) = delete;
    ObjectStorage &operator=(const ObjectStorage &) = delete;

    template <typename... Arguments>
    [[nodiscard]] Status construct(Arguments &&...arguments)
    {
        if (constructed_) return Status::invalid_state;
        ::new (address()) T(static_cast<Arguments &&>(arguments)...);
        constructed_ = true;
        return Status::ok;
    }

    void destroy()
    {
        if (!constructed_) return;
        object().~T();
        constructed_ = false;
    }

    bool     constructed() const { return constructed_; }
    T       &object() { return *static_cast<T *>(address()); }
    const T &object() const { return *static_cast<const T *>(address()); }
    void       *address() { return bytes_; }
    const void *address() const { return bytes_; }

private:
    alignas(T) unsigned char bytes_[sizeof(T)] = {};
    bool constructed_ = false;
};

}  // namespace av

#endif  // AV_OBJECT_STORAGE_HPP
