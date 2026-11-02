#ifndef AV_FAKE_EXPORT_CONSUMER_HPP
#define AV_FAKE_EXPORT_CONSUMER_HPP

#include "core/support/status.hpp"
#include "proto/export/export_answer.hpp"
#include "proto/export/export_request.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class FakeExportConsumer {
public:
    FakeExportConsumer();
    ~FakeExportConsumer();
    FakeExportConsumer(const FakeExportConsumer &) = delete;
    FakeExportConsumer &operator=(const FakeExportConsumer &) = delete;

    int  device_end();
    void refused();

    bool send(const ExportRequest &request);
    bool send_bytes(const std::uint8_t *bytes, std::size_t length);
    Status receive(ExportAnswer &answer, int milliseconds = 5000);
    int    descriptor() const;
    bool   hung_up(int milliseconds = 5000);
    void   leave();

private:
    void forget_descriptor();

    int consumer_;
    int device_;
    int held_;
    int descriptor_;
};

}

#endif
