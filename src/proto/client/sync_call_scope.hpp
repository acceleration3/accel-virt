/* One synchronous call on a ClientTransport, ended when the scope is left.
 *
 * A synchronous reply is decoded from the transport's reply slot after
 * send_sync returns, and the transport has to learn when that decode is over -
 * the ICD's LockedTransport holds its connection's lock until then. The
 * generated clients open one of these just before send_sync, so every return
 * path after it ends the call exactly once (feature 045). */
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

}  // namespace av

#endif  // AV_SYNC_CALL_SCOPE_HPP
