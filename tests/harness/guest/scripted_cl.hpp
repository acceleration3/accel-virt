#ifndef AV_SCRIPTED_CL_HPP
#define AV_SCRIPTED_CL_HPP

#include "core/support/runtime_call.hpp"
#include "guest/probe/cl_entry.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class ScriptedCl final : public ClEntry {
public:
    enum class Call : std::uint32_t {
        platform_count,
        platforms,
        platform_text,
        devices,
        device_text,
        device_number,
        create_context,
        create_queue,
        create_buffer,
        create_buffer_with_properties,
        create_program,
        build_program,
        program_build_text,
        create_kernel,
        program_binary,
        create_program_with_binary,
        create_program_with_il,
        compile_program,
        link_program,
        set_kernel_buffer,
        write_buffer,
        read_buffer,
        run_kernel,
        finish,
        map_buffer,
        unmap,
        svm_alloc,
        set_kernel_svm,
        create_user_event,
        set_user_event_status,
        watch_event,
        wait_for_event,
        release_event,
        release_kernel,
        release_program,
        release_buffer,
        release_queue,
        release_context,
        count
    };

    static constexpr std::uint32_t max_platforms = 12u;
    static constexpr std::uint32_t max_devices = 4u;
    static constexpr std::uint32_t max_objects = 32u;
    static constexpr std::size_t   binary_bytes = 16u;
    static constexpr std::uint32_t max_elements = 64u;
    static constexpr std::uint32_t max_shared_words = 8u;
    static constexpr std::int32_t  refused = -30;
    static constexpr std::uint64_t default_luid = 0x0000000200001234ull;
    static constexpr std::uint64_t default_vendor = 0x1AF4u;

    ScriptedCl();
    ScriptedCl(const ScriptedCl &) = delete;
    ScriptedCl &operator=(const ScriptedCl &) = delete;

    void          fail_at(Call call, std::uint32_t nth, std::int32_t error);
    void          fail_from(Call call, std::uint32_t nth, std::int32_t error);
    void          fail_silently_at(Call call, std::uint32_t nth);
    std::uint32_t calls(Call call) const;

    void          set_platform_suffix(std::uint32_t index, const char *suffix);
    void          add_platform(const char *suffix);
    void          overstate_platforms();
    void          list_no_platforms();
    void          set_device_count(std::uint32_t count);
    void          set_device_extensions(const char *names);
    void          set_luid(std::uint64_t luid);
    void          set_vendor(std::uint64_t vendor);
    void          offer_no_svm();
    void          offer_no_il();
    void          set_build_log(const char *log);
    void          miscompute(std::uint32_t wrong);
    void          stale_mappings(std::uint32_t wrong);
    void          forget_callbacks();
    void          run_callbacks_twice();
    void          defer_callbacks();
    bool          deliver_deferred();

    void          memory_behind(void *handle, const std::uint32_t *words, std::uint32_t count);

    const char   *asked_devices_of() const;
    std::uint32_t live_objects() const;
    std::uint32_t misuses() const;

    std::int32_t platform_count(std::uint32_t &count) override;
    std::int32_t platforms(void **into, std::uint32_t room, std::uint32_t &written) override;
    std::int32_t platform_text(void *platform, std::uint32_t name, char *into, std::size_t room,
                               std::size_t &bytes) override;
    std::int32_t devices(void *platform, void **into, std::uint32_t room,
                         std::uint32_t &written) override;
    std::int32_t device_text(void *device, std::uint32_t name, char *into, std::size_t room,
                             std::size_t &bytes) override;
    std::int32_t device_number(void *device, std::uint32_t name, std::uint64_t &value) override;

    void        *create_context(void *device, std::int32_t &error) override;
    void        *create_queue(void *context, void *device, std::int32_t &error) override;
    void        *create_buffer(void *context, std::uint64_t flags, std::size_t bytes,
                               std::int32_t &error) override;
    void        *create_buffer_with_properties(void *context, const std::uint64_t *properties,
                                               std::uint64_t flags, std::size_t bytes,
                                               std::int32_t &error) override;
    void        *create_program(void *context, const char *const *sources, std::uint32_t count,
                                std::int32_t &error) override;
    std::size_t  program_bytes() const;
    std::int32_t build_program(void *program, void *device, const char *options) override;
    std::int32_t program_build_text(void *program, void *device, char *into, std::size_t room,
                                    std::size_t &bytes) override;
    void        *create_kernel(void *program, const char *name, std::int32_t &error) override;
    std::uint8_t *program_binary(void *program, std::size_t &bytes, std::int32_t &error) override;
    void        *create_program_with_binary(void *context, void *device, const std::uint8_t *binary,
                                            std::size_t bytes, std::int32_t &status, std::int32_t &error) override;
    void        *create_program_with_il(void *context, const void *il, std::size_t bytes,
                                        std::int32_t &error) override;
    std::int32_t compile_program(void *program, void *device, void *header, const char *header_name) override;
    void        *link_program(void *context, void *const *programs, std::uint32_t count,
                              std::int32_t &error) override;

    std::int32_t set_kernel_buffer(void *kernel, std::uint32_t index, void *buffer) override;
    std::int32_t write_buffer(void *queue, void *buffer, std::size_t bytes,
                              const void *from) override;
    std::int32_t read_buffer(void *queue, void *buffer, std::size_t bytes, void *into) override;
    std::int32_t run_kernel(void *queue, void *kernel, std::size_t elements) override;
    std::int32_t finish(void *queue) override;

    void        *map_buffer(void *queue, void *buffer, std::uint64_t flags, std::size_t bytes,
                            std::int32_t &error) override;
    std::int32_t unmap(void *queue, void *buffer, void *pointer) override;

    void        *svm_alloc(void *context, std::size_t bytes) override;
    void         svm_free(void *context, void *pointer) override;
    std::int32_t set_kernel_svm(void *kernel, std::uint32_t index, void *pointer) override;

    void        *create_user_event(void *context, std::int32_t &error) override;
    std::int32_t set_user_event_status(void *event, std::int32_t status) override;
    std::int32_t watch_event(void *event, std::int32_t status,
                             void (*notify)(void *event, std::int32_t status, void *user_data),
                             void *user_data) override;
    std::int32_t wait_for_event(void *event) override;

    void release_event(void *event) override;
    void release_kernel(void *kernel) override;
    void release_program(void *program) override;
    void release_buffer(void *buffer) override;
    void release_queue(void *queue) override;
    void release_context(void *context) override;

private:
    enum class Kind : std::uint32_t { none, context, queue, buffer, program, kernel, event };

    using Notify = void(AV_RUNTIME_CALL *)(void *event, std::int32_t status, void *user_data);

    struct Object {
        Kind          kind = Kind::none;
        bool          live = false;
        float         values[max_elements] = {};
        bool          mapped = false;
        bool          built = false;
        bool          compiled = false;
        void         *arguments[3] = {};
        std::int32_t  status = 0;
        std::int32_t  watched = 0;
        Notify        notify = nullptr;
        void         *user_data = nullptr;
        bool          watching = false;
    };

    std::int32_t failure(Call call);
    bool         answers_nothing(Call call, std::int32_t &error);
    void        *make(Kind kind);
    Object      *required(void *handle, Kind kind);
    void         release(void *handle, Kind kind, Call call);
    std::uint32_t platform_row(void *platform) const;
    bool          is_device(void *device) const;
    std::int32_t  answer_text(const char *text, char *into, std::size_t room,
                              std::size_t &bytes) const;

    std::uint32_t fail_at_[static_cast<std::uint32_t>(Call::count)];
    std::uint32_t silent_at_[static_cast<std::uint32_t>(Call::count)];
    bool          from_[static_cast<std::uint32_t>(Call::count)];
    std::int32_t  errors_[static_cast<std::uint32_t>(Call::count)];
    std::uint32_t calls_[static_cast<std::uint32_t>(Call::count)];
    const char   *suffixes_[max_platforms];
    std::uint32_t platforms_;
    bool          overstating_;
    std::uint32_t devices_;
    const char   *device_name_;
    const char   *device_extensions_;
    const char   *build_log_;
    std::size_t   program_bytes_ = 0u;
    const char   *asked_devices_of_;
    std::uint64_t luid_;
    std::uint64_t vendor_;
    std::uint64_t svm_capabilities_;
    bool          il_ = true;
    std::uint8_t  binary_[binary_bytes] = {};
    std::uint32_t wrong_;
    std::uint32_t stale_;
    bool          forgets_callbacks_;
    bool          notifies_twice_;
    bool          defers_callbacks_;
    Object       *deferred_;
    Object        objects_[max_objects];
    std::uint32_t live_;
    std::uint32_t misuses_;
    void         *exported_handle_;
    std::uint32_t exported_words_[max_shared_words];
    float         shared_[max_elements];
    bool          shared_live_;
};

}

#endif
