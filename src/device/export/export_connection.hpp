#ifndef AV_EXPORT_CONNECTION_HPP
#define AV_EXPORT_CONNECTION_HPP

#include "core/support/status.hpp"
#include "proto/export/export_operation.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class DisplayClock;
class ExportAnswer;
class ExportRequest;
class ExportedGpu;
class Host;
struct ExportConnectionProbe;

class ExportConnection {
public:
    static constexpr uint32_t first_room = 16u;

    ExportConnection(Host &host, const ExportedGpu &gpu, DisplayClock &clock);
    ~ExportConnection();
    ExportConnection(const ExportConnection &) = delete;
    ExportConnection &operator=(const ExportConnection &) = delete;

    [[nodiscard]] Status open(int descriptor);
    void                 serve();
    void                 revoke(uint64_t surface, ExportRevocation reason);
    bool                 serving() const;
    bool                 ended() const;
    void                 close();
    uint32_t             held() const;

    bool invariants_hold() const;

private:
    bool         answer(const uint8_t *bytes, size_t length);
    ExportAnswer respond(const ExportRequest &request, int &descriptor);
    bool         send(const ExportAnswer &answer, int descriptor);
    bool         holds(uint64_t surface) const;
    void         drop(uint64_t surface);
    bool         room_for_one();

    Host              &host_;
    const ExportedGpu &gpu_;
    DisplayClock      &clock_;
    void              *lock_;
    void              *thread_;
    int                descriptor_;
    bool               ended_;
    uint64_t          *surfaces_;
    uint32_t           room_;
    uint32_t           held_;

    friend struct ExportConnectionProbe;
};

}

#endif
