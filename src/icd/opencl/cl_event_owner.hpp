#ifndef AV_CLICD_CL_EVENT_OWNER_HPP
#define AV_CLICD_CL_EVENT_OWNER_HPP

namespace av::clicd {

class ClObject;

class ClEventOwner {
public:
    ClEventOwner(const ClEventOwner &) = delete;
    ClEventOwner &operator=(const ClEventOwner &) = delete;

    virtual void deliver_watched(ClObject &event) = 0;
    virtual void release_watched(ClObject &event) = 0;

protected:
    ClEventOwner() = default;
    ~ClEventOwner() = default;
};

}

#endif
