/* One reference to a COM object, released when this goes or is reset: what a
 * Direct3D or DXGI object the driver made is held by, so no path forgets a
 * Release. Not copyable - a copy would release one reference twice. */
#ifndef AV_UM_COM_REFERENCE_HPP
#define AV_UM_COM_REFERENCE_HPP

#include "core/trace/function_scope.hpp"

namespace av {

template <typename T> class ComReference {
public:
    ComReference();
    ~ComReference();
    ComReference(const ComReference &) = delete;
    ComReference &operator=(const ComReference &) = delete;

    T   *get() const;
    T   *operator->() const;
    /* Where a creating call writes the reference it hands over; whatever was
     * held is released first. */
    T  **receive();
    void reset();

private:
    T *object_;
};

template <typename T> ComReference<T>::ComReference() : object_(nullptr)
{
    AV_SCOPE("ComReference::ComReference");
}

template <typename T> ComReference<T>::~ComReference()
{
    AV_SCOPE("ComReference::~ComReference");
    reset();
}

template <typename T> T *ComReference<T>::get() const
{
    AV_SCOPE("ComReference::get");
    return object_;
}

template <typename T> T *ComReference<T>::operator->() const
{
    AV_SCOPE("ComReference::operator->");
    return object_;
}

template <typename T> T **ComReference<T>::receive()
{
    AV_SCOPE("ComReference::receive");
    reset();
    return &object_;
}

template <typename T> void ComReference<T>::reset()
{
    AV_SCOPE("ComReference::reset");
    if (object_ != nullptr) object_->Release();
    object_ = nullptr;
}

}  // namespace av

#endif  // AV_UM_COM_REFERENCE_HPP
