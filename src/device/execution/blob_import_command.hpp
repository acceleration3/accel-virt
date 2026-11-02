#ifndef AV_BLOB_IMPORT_COMMAND_HPP
#define AV_BLOB_IMPORT_COMMAND_HPP

#include "core/support/status.hpp"
#include "proto/submission/command_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobImports;

class BlobImportCommand {
public:
    static constexpr size_t release_body_size = 8u;

    BlobImportCommand(BlobImports &imports, const CommandHeader &header, const uint8_t *body);

    [[nodiscard]] Status run(uint8_t *reply, size_t capacity, size_t &produced);

private:
    [[nodiscard]] Status import(uint8_t *reply, size_t capacity, size_t &produced);
    [[nodiscard]] Status release();
    size_t               body_size() const;

    BlobImports   &imports_;
    CommandHeader  header_;
    const uint8_t *body_;
};

}

#endif
