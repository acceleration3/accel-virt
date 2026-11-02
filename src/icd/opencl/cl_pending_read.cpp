#include "icd/opencl/cl_pending_read.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClPendingRead::ClPendingRead()
    : into_(nullptr), event_(0u), queue_(0u), buffer_(0u), offset_(0u), bytes_(0u),
      ours_(false), held_(false)
{
    AV_SCOPE("ClPendingRead::ClPendingRead");
}

bool ClPendingRead::take(uint64_t event, uint64_t queue, uint64_t buffer, size_t offset,
                         size_t bytes, void *into, bool ours)
{
    AV_SCOPE("ClPendingRead::take");
    if (event == 0u || queue == 0u || buffer == 0u || bytes == 0u || into == nullptr) return false;
    into_ = into;
    event_ = event;
    queue_ = queue;
    buffer_ = buffer;
    offset_ = offset;
    bytes_ = bytes;
    ours_ = ours;
    held_ = true;
    return true;
}

void ClPendingRead::give_up()
{
    AV_SCOPE("ClPendingRead::give_up");
    into_ = nullptr;
    event_ = 0u;
    queue_ = 0u;
    buffer_ = 0u;
    offset_ = 0u;
    bytes_ = 0u;
    ours_ = false;
    held_ = false;
}

bool     ClPendingRead::held() const { AV_SCOPE("ClPendingRead::held"); return held_; }
uint64_t ClPendingRead::event() const { AV_SCOPE("ClPendingRead::event"); return event_; }
uint64_t ClPendingRead::queue() const { AV_SCOPE("ClPendingRead::queue"); return queue_; }
uint64_t ClPendingRead::buffer() const { AV_SCOPE("ClPendingRead::buffer"); return buffer_; }
size_t   ClPendingRead::offset() const { AV_SCOPE("ClPendingRead::offset"); return offset_; }
size_t   ClPendingRead::bytes() const { AV_SCOPE("ClPendingRead::bytes"); return bytes_; }
void    *ClPendingRead::into() const { AV_SCOPE("ClPendingRead::into"); return into_; }
bool     ClPendingRead::ours() const { AV_SCOPE("ClPendingRead::ours"); return ours_; }

bool ClPendingRead::invariants_hold() const
{
    AV_SCOPE("ClPendingRead::invariants_hold");
    if (held_) return into_ != nullptr && event_ != 0u && queue_ != 0u && buffer_ != 0u && bytes_ != 0u;
    return into_ == nullptr && event_ == 0u && queue_ == 0u && buffer_ == 0u &&
           bytes_ == 0u && !ours_;
}

}
