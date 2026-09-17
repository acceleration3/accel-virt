/* How many values a context's state queries answer where the answer depends on
 * the host (the generated client's pname_count and uniform-block hooks, asked
 * inside a call, where the host cannot be asked): the three format lists whose
 * length is itself a query, learned once when the context is made, and
 * otherwise the registry's table. A count not learned is 0, and nothing is
 * written. */
#ifndef AV_GLICD_QUERY_SHAPES_HPP
#define AV_GLICD_QUERY_SHAPES_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::glicd {

struct QueryShapesProbe;

class QueryShapes {
public:
    enum class List : uint8_t { compressed_texture_formats, program_binary_formats, shader_binary_formats };

    /* The queries that give each list's length, in List's order. */
    static constexpr uint32_t length_queries[] = {0x86A2u, 0x87FEu, 0x8DF9u};
    static constexpr uint32_t lists = 3u;

    QueryShapes();

    void   learn(List list, int32_t length);
    size_t values(uint32_t pname) const;
    /* glGetActiveUniformBlockiv: the active uniform indices are a list whose
     * length is not kept, so 0; every other name answers one value. */
    size_t block_values(uint32_t pname) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t list_queries[lists] = {0x86A3u, 0x87FFu, 0x8DF8u};
    static constexpr uint32_t active_uniform_indices = 0x8A43u;

    uint32_t lengths_[lists];

    friend struct QueryShapesProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_QUERY_SHAPES_HPP
