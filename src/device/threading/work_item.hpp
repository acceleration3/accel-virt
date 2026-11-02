#ifndef AV_WORK_ITEM_HPP
#define AV_WORK_ITEM_HPP

#include "device/host/host.hpp"

namespace av {

class WorkItem {
public:
    WorkItem();
    WorkItem(HostRoutine work, HostRoutine completion, void *context);

    void run() const;
    void complete_on(Host &host) const;

private:
    HostRoutine work_;
    HostRoutine completion_;
    void       *context_;
};

}

#endif
