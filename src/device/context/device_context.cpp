#include "device/context/device_context.hpp"

namespace av {

DeviceContext::DeviceContext(Host &host, uint32_t id) : id_(id), worker_(host) {}

Status DeviceContext::start()
{
    return worker_.start();
}

Status DeviceContext::post(const WorkItem &item)
{
    return worker_.post(item);
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
