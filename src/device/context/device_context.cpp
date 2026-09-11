#include "device/context/device_context.hpp"

namespace av {

DeviceContext::DeviceContext(Host &host, BlobTable &blobs, uint32_t id)
    : id_(id), worker_(host), binding_(host, blobs) {}

Status DeviceContext::start()
{
    return worker_.start();
}

Status DeviceContext::post(const WorkItem &item)
{
    return worker_.post(item);
}

Status DeviceContext::bind(const ContextSetup &setup, const CapsetInfo &capset)
{
    return binding_.bind(setup, capset);
}

const ContextBinding &DeviceContext::binding() const
{
    return binding_;
}

uint32_t DeviceContext::id() const
{
    return id_;
}

bool DeviceContext::running() const
{
    return worker_.running();
}

}  // namespace av
