#ifndef AV_CLICD_CL_DESTRUCTOR_CALLBACKS_HPP
#define AV_CLICD_CL_DESTRUCTOR_CALLBACKS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::clicd {

struct ClDestructorCallbacksProbe;

class ClDestructorCallbacks {
public:
    using MemCallback = void(CL_CALLBACK *)(cl_mem object, void *user_data);
    using ContextCallback = void(CL_CALLBACK *)(cl_context object, void *user_data);

    static constexpr uint32_t max_gone = 8u;

private:
    struct Row {
        const void     *object_;
        MemCallback     mem_;
        ContextCallback context_;
        void           *user_data_;
    };

public:
    class Due {
    public:
        explicit Due(BlockSource &memory);
        Due(const Due &) = delete;
        Due &operator=(const Due &) = delete;

        void     run() const;
        uint32_t count() const;
        uint32_t lost() const;

    private:
        friend class ClDestructorCallbacks;

        GrowableArray<Row> rows_;
        uint32_t           lost_;
    };

    explicit ClDestructorCallbacks(BlockSource &memory);
    ClDestructorCallbacks(const ClDestructorCallbacks &) = delete;
    ClDestructorCallbacks &operator=(const ClDestructorCallbacks &) = delete;

    [[nodiscard]] Status keep(cl_mem object, MemCallback callback, void *user_data);
    [[nodiscard]] Status keep(cl_context object, ContextCallback callback, void *user_data);

    void take(const void *const *gone, uint32_t count, Due &due);

    uint32_t kept() const;
    bool     invariants_hold() const;

private:
    friend struct ClDestructorCallbacksProbe;

    [[nodiscard]] Status keep_row(const Row &row);
    bool                 names_gone(const Row &row, const void *const *gone, uint32_t count) const;

    GrowableArray<Row> rows_;
};

}

#endif
