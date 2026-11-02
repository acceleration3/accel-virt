#ifndef AV_SYNC_CALL_SCOPE_HPP
#define AV_SYNC_CALL_SCOPE_HPP

namespace av {

class ClientTransport;

class SyncCallScope {
public:
    explicit SyncCallScope(ClientTransport &transport);
    ~SyncCallScope();
    SyncCallScope(const SyncCallScope &) = delete;
    SyncCallScope &operator=(const SyncCallScope &) = delete;

private:
    ClientTransport &transport_;
};

}

#endif
