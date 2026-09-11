/* One DXGKARG_QUERYADAPTERINFO request. Its buffers belong to the caller, so
 * every read and write is checked against the sizes the caller declared. */
#ifndef AV_ADAPTER_INFO_QUERY_HPP
#define AV_ADAPTER_INFO_QUERY_HPP

#include "platform.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

struct DxgkargQueryAdapterInfoRecord;

class AdapterInfoQuery {
public:
    AdapterInfoQuery(Platform &platform, const DxgkargQueryAdapterInfoRecord &record);
    AdapterInfoQuery(const AdapterInfoQuery &) = delete;
    AdapterInfoQuery &operator=(const AdapterInfoQuery &) = delete;

    int32_t type() const;
    /* invalid_parameter unless the caller supplied at least `bytes` of input. */
    [[nodiscard]] Status require_input(uint32_t bytes) const;
    /* Copies the first `count` input bytes; invalid_parameter, copying nothing,
     * unless the caller supplied that many. */
    [[nodiscard]] Status read_input(void *destination, uint32_t count) const;
    /* The same for an output block the caller also fills in. */
    [[nodiscard]] Status read_output(void *destination, uint32_t count) const;
    /* Copies exactly `count` bytes to the start of the output; a null output
     * or a declared size below `count` is invalid_parameter and writes nothing. */
    [[nodiscard]] Status write_output(const void *bytes, uint32_t count) const;

private:
    Platform                            &platform_;
    const DxgkargQueryAdapterInfoRecord &record_;
};

}  // namespace av

#endif  // AV_ADAPTER_INFO_QUERY_HPP
