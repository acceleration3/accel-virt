#ifndef AV_OBJECT_STORAGE_HPP
#define AV_OBJECT_STORAGE_HPP

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

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
        AV_SCOPE("ObjectStorage::construct");
        if (constructed_) return Status::invalid_state;
        ::new (address()) T(static_cast<Arguments &&>(arguments)...);
        constructed_ = true;
        return Status::ok;
    }

    void destroy()
    {
        AV_SCOPE("ObjectStorage::destroy");
        if (!constructed_) return;
        constructed_ = false;
        object().~T();
    }

    void forget()
    {
        AV_SCOPE("ObjectStorage::forget");
        constructed_ = false;
    }

    bool     constructed() const
    {
        AV_SCOPE("ObjectStorage::constructed");
        return constructed_;
    }
    T       &object()
    {
        AV_SCOPE("ObjectStorage::object");
        return *static_cast<T *>(address());
    }
    const T &object() const
    {
        AV_SCOPE("ObjectStorage::object");
        return *static_cast<const T *>(address());
    }
    void       *address()
    {
        AV_SCOPE("ObjectStorage::address");
        return bytes_;
    }
    const void *address() const
    {
        AV_SCOPE("ObjectStorage::address");
        return bytes_;
    }

private:
    alignas(T) unsigned char bytes_[sizeof(T)] = {};
    bool constructed_ = false;
};

}

#endif
