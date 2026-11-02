#ifndef AV_FAKE_OPENGL32_HPP
#define AV_FAKE_OPENGL32_HPP

#include "icd/opengl/gl_entry_points.hpp"
#include "icd/opengl/system_opengl.hpp"

#include <cstdint>

namespace av {

class ScriptedThreadSlot;

class FakeOpengl32 final : public glicd::SystemOpengl {
public:
    enum class Load : std::uint8_t { loaded, missing_entry, refused_version };
    static constexpr std::uint32_t max_contexts = 32u;
    static constexpr std::uint32_t callbacks = 9u;
    static constexpr std::uint32_t full_table = 336u;
    static constexpr std::uint32_t short_table = 306u;

    explicit FakeOpengl32(ScriptedThreadSlot &threads);
    ~FakeOpengl32();
    FakeOpengl32(const FakeOpengl32 &) = delete;
    FakeOpengl32 &operator=(const FakeOpengl32 &) = delete;

    void refuse_next_create();
    void skip_driver_next_create();
    void hide_export(const char *name);
    Load load(std::uint32_t version, std::uint32_t flags);

    std::int32_t describe_pixel_format(void *dc, std::int32_t index, std::uint32_t bytes, std::uint8_t *descriptor);
    bool         set_pixel_format(void *dc, std::int32_t index);
    void        *create_context(void *dc) override;
    bool         make_current(void *dc, void *context);
    bool         delete_context(void *context);
    bool         share_lists(void *first, void *second);
    bool         swap_buffers(void *dc);
    void        *get_proc_address(const char *name);
    void        *driver_proc_address(const char *name);
    std::uintptr_t driver_context(void *context) const;
    void        *slot(std::uint32_t index) const;
    std::uint32_t table_entries() const;
    std::uint32_t dhglrc_lookups() const;
    void          count_lookup();

    template <typename Function>
    Function gl(std::uint32_t index) const
    {
        return reinterpret_cast<Function>(slot(index));
    }

private:
    struct Table {
        std::int32_t entries;
        void        *slots[full_table];
    };
    void *resolve(const char *name) const;

    ScriptedThreadSlot   &threads_;
    glicd::DriverExports  exports_;
    const char           *hidden_[4];
    std::uint32_t         hidden_count_;
    std::uintptr_t        contexts_[max_contexts];
    const Table          *tables_[8];
    std::uintptr_t        current_[8];
    std::uint32_t         lookups_;
    bool                  refuse_create_;
    bool                  skip_driver_;
    void                 *procs_[callbacks];
};

}

#endif
