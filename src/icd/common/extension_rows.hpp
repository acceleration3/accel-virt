#ifndef AV_ICD_EXTENSION_ROWS_HPP
#define AV_ICD_EXTENSION_ROWS_HPP

#include "icd/common/extension_row.hpp"

#include <stdint.h>

namespace av::icd {

class ExtensionRows {
public:
    static constexpr uint32_t name_bytes = 256u;

    class Names {
    public:
        Names(const char *const *names, uint32_t count);

        bool holds(const char *name) const;

    private:
        const char *const *names_;
        uint32_t           count_;
    };

    ExtensionRows(const ExtensionRow *rows, uint32_t count, const char *prefix, Names carried, Names withheld);
    ExtensionRows(const ExtensionRow *rows, uint32_t count, const char *prefix, Names withheld);
    ExtensionRows(const ExtensionRows &) = delete;
    ExtensionRows &operator=(const ExtensionRows &) = delete;

    uint32_t            count() const;
    const ExtensionRow &row(uint32_t index) const;
    bool                reportable(uint32_t index) const;
    uint32_t            named(const char *name) const;
    uint32_t            answering(const char *host_name, uint32_t from) const;
    const char         *host_name(uint32_t index) const;
    uint32_t            version(uint32_t index, uint32_t host_version) const;

    bool invariants_hold() const;

private:
    bool     same(const char *ours, const char *name) const;
    uint32_t letters_of(const char *name) const;
    bool     row_holds(const ExtensionRow &row) const;

    const ExtensionRow *rows_;
    uint32_t            count_;
    const char         *prefix_;
    Names               carried_;
    Names               withheld_;
    bool                own_;
};

}

#endif
