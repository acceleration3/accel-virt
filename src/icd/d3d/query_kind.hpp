#ifndef AV_D3D_QUERY_KIND_HPP
#define AV_D3D_QUERY_KIND_HPP

#include <stdint.h>

namespace av::d3d {

class QueryKind {
public:
    explicit QueryKind(uint32_t ddi_type);

    [[nodiscard]] bool     known() const;
    [[nodiscard]] uint32_t api_type() const;
    [[nodiscard]] uint32_t data_bytes() const;

private:
    uint32_t api_type_;
    uint32_t data_bytes_;
};

}

#endif
