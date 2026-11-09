#ifndef AV_OWNED_FD_HPP
#define AV_OWNED_FD_HPP

#include <stdint.h>

namespace av {

class OwnedFdIdentity {
public:
    OwnedFdIdentity() : device_(0u), inode_(0u), known_(false) {}
    OwnedFdIdentity(uint64_t device, uint64_t inode) : device_(device), inode_(inode), known_(true) {}

    bool known() const { return known_; }
    bool same(const OwnedFdIdentity &other) const
    {
        return known_ && other.known_ && device_ == other.device_ && inode_ == other.inode_;
    }

private:
    uint64_t device_;
    uint64_t inode_;
    bool     known_;
};

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
    OwnedFdIdentity    identity() const;
    void               settle_failed_import(const OwnedFdIdentity &before);

private:
    int fd_;
};

}

#endif
