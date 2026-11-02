#ifndef AV_SCRIPTED_KERNEL_TRACE_HPP
#define AV_SCRIPTED_KERNEL_TRACE_HPP

#include "harness/guest/mock_kmt.hpp"
#include "proto/escape/trace_escape_record.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class ScriptedKernelTrace final : public AdapterEscapes {
public:
    enum class Fault : std::uint8_t { none, short_window, grown_total, refused, other_offset, zero_window };

    ScriptedKernelTrace();

    std::uint32_t answer(std::uint8_t *data, std::uint32_t size) override;

    void          serve(TraceEscapeRecord::Kind kind, const std::uint8_t *bytes, std::size_t length);
    void          claim_total(std::uint64_t total);
    void          set_clock(std::uint64_t now);
    void          fault_at(std::uint32_t nth, Fault fault);
    std::uint32_t answers() const;

private:
    const std::uint8_t *table_;
    std::size_t         table_length_;
    const std::uint8_t *state_;
    std::size_t         state_length_;
    std::uint64_t       claimed_;
    bool                claiming_;
    std::uint64_t       now_;
    std::uint32_t       answers_;
    std::uint32_t       fault_at_;
    Fault               fault_;
};

}

#endif
