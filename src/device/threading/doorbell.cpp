#include "device/threading/doorbell.hpp"

#include "device/host/host.hpp"

namespace av {

Doorbell::Doorbell(Host &host) : host_(host), event_(nullptr), index_(0u), connected_(false) {}

Doorbell::~Doorbell() { disconnect(); }

Status Doorbell::connect(void *event, uint32_t index)
{
    host_.require_main_loop("Doorbell::connect");
    if (connected_) return Status::invalid_state;
    if (event == nullptr || index >= pages) return Status::invalid_parameter;
    host_.connect_doorbell(event, index);
    event_ = event;
    index_ = index;
    connected_ = true;
    return Status::ok;
}

void Doorbell::disconnect()
{
    host_.require_main_loop("Doorbell::disconnect");
    if (!connected_) return;
    host_.disconnect_doorbell(event_, index_);
    event_ = nullptr;
    connected_ = false;
}

bool Doorbell::connected() const
{
    return connected_;
}

uint32_t Doorbell::index() const
{
    return index_;
}

bool Doorbell::invariants_hold() const
{
    return connected_ == (event_ != nullptr) && index_ < pages;
}

}  // namespace av
