#ifndef AV_CONTEXT_REQUEST_SEQUENCE_HPP
#define AV_CONTEXT_REQUEST_SEQUENCE_HPP

#include "core/platform/platform.hpp"
#include "miniport/av_context/context_pages.hpp"
#include "core/support/status.hpp"
#include "proto/context/context_setup.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct ContextRequestSequenceProbe;

class ContextRequestSequence {
public:
    explicit ContextRequestSequence(Platform &platform);

    void                 describe_process();
    [[nodiscard]] Status begin_setup(uint32_t context_id, const ContextSetup &setup, const ContextPages &pages);
    [[nodiscard]] Status begin_teardown();
    [[nodiscard]] Status next_request(uint8_t *bytes, size_t capacity, size_t &length);
    [[nodiscard]] Status accept(const uint8_t *bytes, size_t length);
    void                 abandon();
    bool                 finished() const;
    bool                 refused() const;
    uint32_t             refusal() const;

    bool invariants_hold() const;

private:
    enum class Phase : uint8_t { idle, setting_up, set_up, tearing_down };
    static constexpr uint32_t setup_steps = 5u;
    static constexpr uint32_t blob_count = 3u;
    static constexpr uint32_t context_step = 0u;
    static constexpr uint32_t first_blob_step = 1u;
    static constexpr uint32_t setup_step = 4u;
    static constexpr uint32_t unreadable = 0u;
    static constexpr size_t   image_bytes = 48u;
    static constexpr size_t   label_bytes = 64u;

    [[nodiscard]] Status write_setup_request(uint8_t *bytes, size_t capacity, size_t &length);
    [[nodiscard]] Status write_teardown_request(uint8_t *bytes, size_t capacity, size_t &length);
    void                 finish_setup_step(bool done);
    void                 refuse(uint32_t type);
    uint32_t             teardown_steps() const;

    Platform    &platform_;
    Phase        phase_;
    ContextSetup setup_;
    uint32_t     context_id_;
    ContextPages pages_;
    uint32_t     step_;
    bool         awaiting_;
    bool         context_created_;
    uint32_t     blobs_created_;
    uint32_t     refusal_;
    bool         refused_;
    char         label_[label_bytes];
    uint32_t     label_length_;

    friend struct ContextRequestSequenceProbe;
};

}

#endif
