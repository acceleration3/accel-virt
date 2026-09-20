/* The OpenCL the CL probe's unit tests play (feature 052, T029), behind
 * ClEntry: platforms that answer an ICD suffix, one device with the adapter's
 * LUID, and objects handed out as pointers this alone recognises - a handle it
 * never made, or made and released, is refused and counted. A kernel run over
 * three bound buffers really adds them, so what read_buffer and map_buffer
 * answer is what the arithmetic produced rather than what the probe hoped for.
 * Any call can be made to answer an error on its Nth use, every object made is
 * counted so a test asserts nothing is left, and a user event whose status is
 * set runs the callback registered on it.
 */
#ifndef AV_SCRIPTED_CL_HPP
#define AV_SCRIPTED_CL_HPP

#include "guest/suite/cl_entry.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class ScriptedCl final : public ClEntry {
public:
    /* The calls a test can fail, and count. */
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

    /* Room for more than the probe reads: its own walk stops at eight, and
     * that bound is what keeps it inside its array whatever a loader lists. */
    static constexpr std::uint32_t max_platforms = 12u;
    static constexpr std::uint32_t max_devices = 4u;
    static constexpr std::uint32_t max_objects = 16u;
    /* As many as ClProbe's vector add has. */
    static constexpr std::uint32_t max_elements = 64u;
    /* As many words of an exported allocation as the shared-object probe reads
     * back out of it. */
    static constexpr std::uint32_t max_shared_words = 8u;
    /* CL_INVALID_VALUE: what a call this refuses answers. */
    static constexpr std::int32_t  refused = -30;
    /* The LUID and vendor a device carries until a test says otherwise. */
    static constexpr std::uint64_t default_luid = 0x0000000200001234ull;
    static constexpr std::uint64_t default_vendor = 0x1AF4u;

    ScriptedCl();
    ScriptedCl(const ScriptedCl &) = delete;
    ScriptedCl &operator=(const ScriptedCl &) = delete;

    /* The Nth use (1-based) of `call` answers `error`, which stands in for the
     * null a creating call returns; zero means this object's own refusal. */
    void          fail_at(Call call, std::uint32_t nth, std::int32_t error);
    /* The same, but from the nth call onward rather than at it alone: what a
     * host that has run out of something looks like, where the next ask
     * fails too. */
    void          fail_from(Call call, std::uint32_t nth, std::int32_t error);
    /* A call that answers nothing and says nothing: the entry gives back no
     * object and leaves the error where it was. A caller that read that word
     * would read success out of a failure, which is what the probe's steps
     * guard against. Only the entries that answer an object have this. */
    void          fail_silently_at(Call call, std::uint32_t nth);
    std::uint32_t calls(Call call) const;

    /* The platforms a loader lists, in the order it lists them: the first is
     * this driver's until a test renames it, and any this appends are the
     * other vendors' the search must walk past. */
    void          set_platform_suffix(std::uint32_t index, const char *suffix);
    void          add_platform(const char *suffix);
    /* A loader that says it wrote more platforms than it was given room for.
     * Nothing stops a real one doing this, and the probe's own bound is what
     * keeps it inside its array when one does. */
    void          overstate_platforms();
    /* A loader with no ICD registered at all, which lists nothing. */
    void          list_no_platforms();
    void          set_device_count(std::uint32_t count);
    void          set_device_name(const char *name);
    void          set_luid(std::uint64_t luid);
    void          set_vendor(std::uint64_t vendor);
    /* A device with no shared virtual memory, which is a device rather than a
     * failure. */
    void          offer_no_svm();
    /* What a build that failed says it failed for. */
    void          set_build_log(const char *log);
    /* The kernel writes the wrong answer into its first `wrong` elements: a
     * device that ran something other than the program it was given. */
    void          miscompute(std::uint32_t wrong);
    /* The mapping shows `wrong` elements that are not what the device wrote:
     * a map that handed back stale host bytes. */
    void          stale_mappings(std::uint32_t wrong);
    /* A callback registered and then never run, and one run twice: the two
     * ways an ICD's event thread gets a callback wrong. */
    void          forget_callbacks();
    void          run_callbacks_twice();
    /* A callback the event thread has not got to yet when the wait on its
     * event returns, which is what a real ICD's does whenever the waiting
     * thread wins the race: setting the event runs nothing, and `deliver`
     * is the thread catching up. False when there was nothing to run. */
    void          defer_callbacks();
    bool          deliver_deferred();

    /* The memory an exported NT handle names, as whoever exported it left it.
     * A buffer created over that handle reads these words back; one created
     * over any other handle is refused, which is what an ICD does with a
     * handle it never opened (feature 053). */
    void          memory_behind(void *handle, const std::uint32_t *words, std::uint32_t count);

    /* The suffix of the platform the run asked for devices of: which of the
     * ones a loader listed the search settled on. */
    const char   *asked_devices_of() const;
    /* Objects made and not released. */
    std::uint32_t live_objects() const;
    /* Releases of a handle this never made, or already took back, and calls
     * made with objects belonging to another run. */
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
    void        *create_program(void *context, const char *source, std::int32_t &error) override;
    std::int32_t build_program(void *program, void *device, const char *options) override;
    std::int32_t program_build_text(void *program, void *device, char *into, std::size_t room,
                                    std::size_t &bytes) override;
    void        *create_kernel(void *program, const char *name, std::int32_t &error) override;

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

    using Notify = void (*)(void *event, std::int32_t status, void *user_data);

    struct Object {
        Kind          kind = Kind::none;
        bool          live = false;
        /* A buffer's bytes, as floats because that is all a run puts in
         * them, and whether one of them is mapped right now. */
        float         values[max_elements] = {};
        bool          mapped = false;
        /* A program that built, and a kernel's bound arguments. */
        bool          built = false;
        void         *arguments[3] = {};
        /* An event's status, and what waits on it. */
        std::int32_t  status = 0;
        std::int32_t  watched = 0;
        Notify        notify = nullptr;
        void         *user_data = nullptr;
        bool          watching = false;
    };

    /* Zero on success, the error the test asked for on the failing use. */
    std::int32_t failure(Call call);
    /* Whether this call answers nothing, setting `error` to what it says -
     * which may be nothing at all. */
    bool         answers_nothing(Call call, std::int32_t &error);
    void        *make(Kind kind);
    /* The object behind a handle this made, or null and a misuse. */
    Object      *required(void *handle, Kind kind);
    void         release(void *handle, Kind kind, Call call);
    std::uint32_t platform_row(void *platform) const;
    bool          is_device(void *device) const;
    /* `text` into `into`, terminated, with what it needed in `bytes`. */
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
    const char   *build_log_;
    const char   *asked_devices_of_;
    std::uint64_t luid_;
    std::uint64_t vendor_;
    std::uint64_t svm_capabilities_;
    std::uint32_t wrong_;
    std::uint32_t stale_;
    bool          forgets_callbacks_;
    bool          notifies_twice_;
    bool          defers_callbacks_;
    /* The event whose callback was registered and whose status has been set,
     * waiting for the thread that runs it. */
    Object       *deferred_;
    Object        objects_[max_objects];
    std::uint32_t live_;
    std::uint32_t misuses_;
    /* The NT handle a buffer can be opened over, and what the memory behind
     * it holds. */
    void         *exported_handle_;
    std::uint32_t exported_words_[max_shared_words];
    /* The one shared allocation svm_alloc hands out, and who holds it. */
    float         shared_[max_elements];
    bool          shared_live_;
    void         *shared_argument_;
};

}  // namespace av

#endif  // AV_SCRIPTED_CL_HPP
