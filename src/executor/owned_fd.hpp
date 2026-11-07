#ifndef AV_OWNED_FD_HPP
#define AV_OWNED_FD_HPP

#include <stdint.h>

namespace av {

class OwnedFd {
public:
    OwnedFd();
    explicit OwnedFd(int fd);
    ~OwnedFd();
    OwnedFd(const OwnedFd &) = delete;
    OwnedFd &operator=(const OwnedFd &) = delete;
    OwnedFd(OwnedFd &&) = delete;
    OwnedFd &operator=(OwnedFd &&) = delete;

    void               hold(int fd);
    [[nodiscard]] bool hold_copy(int fd);
    [[nodiscard]] bool copy_to(OwnedFd &copy) const;
    int                fd() const;
    uint64_t           extent() const;
    bool               held() const;
    int                take();
    void               close();

private:
    int fd_;
};

}

#endif
