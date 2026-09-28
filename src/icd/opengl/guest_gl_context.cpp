#include "icd/opengl/guest_gl_context.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/scope_exit.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/common/icd_connection.hpp"
#include "icd/opengl/gl_extension_policy.hpp"
#include "proto/context/sub_api.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

namespace {

constexpr uint32_t latest_major = 4u;
constexpr uint32_t latest_minor = 6u;
constexpr uint32_t no_error = 0u;

}  // namespace

GuestGlContext::GuestGlContext(icd::IcdConnection &connection, BlockSource &memory, icd::ThreadLock &send_lock,
                               icd::LogSink &log, const GlExtensionPolicy &extensions,
                               icd::SharedHandles &handles, int32_t pixel_format,
                               const GlContextOptions &options)
    : connection_(connection), memory_(memory), send_lock_(send_lock), log_(log), policy_(extensions),
      pixel_format_(pixel_format),
      options_(options), state_(State::created), owner_(0u), current_dc_(nullptr), error_(no_error), share_target_(0u), link_(icd::IcdConnection::no_link), loss_reported_(false),
      staging_(nullptr), batch_block_(nullptr), transport_(), batched_(), arena_(), arrays_(), uniforms_(), queries_(), profile_(), interval_(), pacer_(), extensions_(), framebuffers_(memory, log), shadows_(memory), mappings_(memory, shadows_, log), externals_(handles, log),
      strings_{},
      debug_callback_(0u), debug_user_(0u)
{
    AV_SCOPE("GuestGlContext::GuestGlContext");
}

GuestGlContext::~GuestGlContext()
{
    AV_SCOPE("GuestGlContext::~GuestGlContext");
    teardown();
}

Status GuestGlContext::make()
{
    AV_SCOPE("GuestGlContext::make");
    if (state_ != State::created) return Status::invalid_state;
    if (!asked_for_itself()) return make_once();
    /* A context created without attributes is what an application written
     * before profiles existed makes, and what it wants is the fixed-function
     * pipeline. So it asks for a compatibility context, then for the driver's
     * own default, then for core - and reports whichever it got (feature 051,
     * research.md §2). Only a refusal of the options moves down the ladder. */
    static constexpr struct Rungs {
        GrantedProfile::Rung      rung;
        uint32_t                  major;
        uint32_t                  minor;
        GlContextOptions::Profile profile;
    } ladder[] = {
        {GrantedProfile::Rung::compatibility_46, latest_major, latest_minor, GlContextOptions::Profile::compatibility},
        {GrantedProfile::Rung::host_default, 0u, 0u, GlContextOptions::Profile::none},
        {GrantedProfile::Rung::core_46, latest_major, latest_minor, GlContextOptions::Profile::core},
    };
    Status asked = Status::invalid_parameter;
    for (const Rungs &rung : ladder) {
        options_ = GlContextOptions(static_cast<uint8_t>(rung.major), static_cast<uint8_t>(rung.minor), rung.profile,
                                    options_.flags(), options_.share_context());
        profile_.granted(rung.rung);
        asked = make_once();
        if (asked != Status::invalid_parameter) return asked;
    }
    profile_.granted(GrantedProfile::Rung::none);
    return asked;
}

Status GuestGlContext::make_once()
{
    AV_SCOPE("GuestGlContext::make_once");
    ScopeExit    undo([&] { teardown(); });
    const Status built = build();
    if (built != Status::ok) return built;
    if (!learn()) {
        log_.write("rvkgl: the host refused the context's options");
        return Status::invalid_parameter;
    }
    undo.dismiss();
    state_ = State::made;
    return Status::ok;
}

/* Whether the application said what it wanted. A version is the thing that
 * says so: wglCreateContextAttribsARB names one, and a context that shares is
 * given 1.0 by `share` before it is made - only a plain wglCreateContext
 * arrives here with none, which is the context the ladder is for. */
bool GuestGlContext::asked_for_itself() const
{
    AV_SCOPE("GuestGlContext::asked_for_itself");
    return options_.major() == 0u;
}

GuestGlContext::State GuestGlContext::state() const
{
    AV_SCOPE("GuestGlContext::state");
    return state_;
}
uint32_t              GuestGlContext::owner() const
{
    AV_SCOPE("GuestGlContext::owner");
    return owner_;
}
void                  GuestGlContext::set_owner(uint32_t thread)
{
    AV_SCOPE("GuestGlContext::set_owner");
    owner_ = thread;
}

void *GuestGlContext::current_dc() const
{
    AV_SCOPE("GuestGlContext::current_dc");
    return current_dc_;
}

void GuestGlContext::set_current_dc(void *dc)
{
    AV_SCOPE("GuestGlContext::set_current_dc");
    current_dc_ = dc;
}
int32_t               GuestGlContext::pixel_format() const
{
    AV_SCOPE("GuestGlContext::pixel_format");
    return pixel_format_;
}
const GlContextOptions &GuestGlContext::options() const
{
    AV_SCOPE("GuestGlContext::options");
    return options_;
}

uint32_t GuestGlContext::share_name() const
{
    AV_SCOPE("GuestGlContext::share_name");
    return state_ == State::created ? 0u : transport_.object().ring();
}

Status GuestGlContext::share_after(uint64_t target)
{
    AV_SCOPE("GuestGlContext::share_after");
    if (state_ != State::created || share_target_ != 0u) return Status::invalid_state;
    share_target_ = target;
    return Status::ok;
}

uint64_t GuestGlContext::share_target() const
{
    AV_SCOPE("GuestGlContext::share_target");
    return share_target_;
}

Status GuestGlContext::share(uint32_t share_name)
{
    AV_SCOPE("GuestGlContext::share");
    if (state_ != State::created) return Status::invalid_state;
    if (options_.major() == 0u) options_ = GlContextOptions(1u, 0u, GlContextOptions::Profile::none, 0u, 0u);
    options_ = options_.sharing(share_name);
    shadows_.distrust();
    return Status::ok;
}

void GuestGlContext::flush()
{
    AV_SCOPE("GuestGlContext::flush");
    if (state_ != State::made || !batched_.constructed()) return;
    send_lock_.acquire();
    const TransportResult sent = batched_.object().flush();
    send_lock_.release();
    if (sent != TransportResult::ok) log_.write("rvkgl: the held calls could not be sent");
}

ClientTransport *GuestGlContext::wire()
{
    AV_SCOPE("GuestGlContext::wire");
    if (state_ != State::made) return nullptr;
    /* One faulted transport loses its whole device: every context on the
     * link finds out at its own next call, on its own thread. */
    if (transport_.object().faulted()) connection_.lose(link_);
    if (connection_.lost(link_)) {
        state_ = State::lost;
        log_.write("rvkgl: the context's device faulted; the context is lost");
    }
    /* The batch itself, with no lock: WGL lets a context be current on one
     * thread at a time, and it is only ever called on that one. The lock
     * every call took serialised each context's calls against every other
     * context's, and cost a sixth of a draw-heavy thread's time. */
    return state_ == State::made ? &batched_.object() : nullptr;
}

ClientArrayState &GuestGlContext::arrays()
{
    AV_SCOPE("GuestGlContext::arrays");
    return arrays_;
}
UniformShapes    &GuestGlContext::uniforms()
{
    AV_SCOPE("GuestGlContext::uniforms");
    return uniforms_;
}
QueryShapes      &GuestGlContext::queries()
{
    AV_SCOPE("GuestGlContext::queries");
    return queries_;
}

const GlExtensionList &GuestGlContext::extensions() const
{
    AV_SCOPE("GuestGlContext::extensions");
    return extensions_;
}

MappedBuffers &GuestGlContext::mappings()
{
    AV_SCOPE("GuestGlContext::mappings");
    return mappings_;
}

BufferShadows &GuestGlContext::shadows()
{
    AV_SCOPE("GuestGlContext::shadows");
    return shadows_;
}

void GuestGlContext::lend()
{
    AV_SCOPE("GuestGlContext::lend");
    shadows_.distrust();
}

GlExternalObjects &GuestGlContext::externals()
{
    AV_SCOPE("GuestGlContext::externals");
    return externals_;
}

WindowFramebuffers &GuestGlContext::framebuffers()
{
    AV_SCOPE("GuestGlContext::framebuffers");
    return framebuffers_;
}

void GuestGlContext::record(uint32_t error)
{
    AV_SCOPE("GuestGlContext::record");
    if (error_ == no_error) error_ = error;
}

uint32_t GuestGlContext::take_error()
{
    AV_SCOPE("GuestGlContext::take_error");
    const uint32_t taken = error_;
    error_ = no_error;
    if (taken != no_error || state_ != State::lost || loss_reported_) return taken;
    loss_reported_ = true;
    return robust() ? gl_context_lost : gl_out_of_memory;
}

const char *GuestGlContext::string(uint32_t name) const
{
    AV_SCOPE("GuestGlContext::string");
    if (state_ == State::created) return nullptr;
    for (uint32_t row = 0u; row < strings; row++)
        if (string_names[row] == name) return strings_[row];
    return nullptr;
}

SwapInterval &GuestGlContext::interval()
{
    AV_SCOPE("GuestGlContext::interval");
    return interval_;
}

void GuestGlContext::pace()
{
    AV_SCOPE("GuestGlContext::pace");
    if (state_ != State::made) return;
    for (uint32_t blank = 0u; blank < interval_.blanks(); blank++) {
        pacer_.object().wait();
        interval_.waited_one();
    }
}

const GrantedProfile &GuestGlContext::profile() const
{
    AV_SCOPE("GuestGlContext::profile");
    return profile_;
}

void GuestGlContext::asking(GrantedProfile::Rung rung)
{
    AV_SCOPE("GuestGlContext::asking");
    profile_.granted(rung);
}

void GuestGlContext::set_debug_callback(uint64_t callback, uint64_t user)
{
    AV_SCOPE("GuestGlContext::set_debug_callback");
    debug_callback_ = callback;
    debug_user_ = user;
}

uint64_t GuestGlContext::debug_callback() const
{
    AV_SCOPE("GuestGlContext::debug_callback");
    return debug_callback_;
}
uint64_t GuestGlContext::debug_user() const
{
    AV_SCOPE("GuestGlContext::debug_user");
    return debug_user_;
}

bool GuestGlContext::robust() const
{
    AV_SCOPE("GuestGlContext::robust");
    const uint8_t both = GlContextOptions::robust_access | GlContextOptions::lose_context_on_reset;
    return (options_.flags() & both) == both;
}

uint32_t GuestGlContext::lost_reset_status() const
{
    AV_SCOPE("GuestGlContext::lost_reset_status");
    return state_ == State::lost && robust() ? gl_unknown_context_reset : no_error;
}

void GuestGlContext::report(StateWriter &writer, uint64_t handle) const
{
    AV_SCOPE("GuestGlContext::report");
    static constexpr const char *const state_names[] = {"created", "made", "lost"};
    writer.open("context", handle);
    writer.text("state", state_names[static_cast<uint32_t>(state_)]);
    writer.number("owner", owner_);
    writer.number("pixel_format", static_cast<uint32_t>(pixel_format_));
    writer.number("major", options_.major());
    writer.number("minor", options_.minor());
    writer.number("profile", static_cast<uint32_t>(options_.profile()));
    writer.number("flags", options_.flags());
    writer.number("share", options_.share_context());
    writer.number("share_target", share_target_);
    writer.number("error", error_);
    writer.number("link", link_);
    writer.flag("arena", arena_.constructed() && arena_.object().lending());
    writer.text("renderer", state_ == State::created ? "" : strings_[renderer_row]);
    writer.number("extensions", extensions_.count());
    framebuffers_.report(writer);
    mappings_.report(writer);
    shadows_.report(writer);
    writer.close();
}

/* Made or lost is exactly when the host context's pieces are held, and a
 * string kept is terminated inside its row. */
bool GuestGlContext::invariants_hold() const
{
    AV_SCOPE("GuestGlContext::invariants_hold");
    const bool held = state_ != State::created;
    if (held != transport_.constructed() || held != batched_.constructed() ||
        held != (link_ != icd::IcdConnection::no_link))
        return false;
    if (loss_reported_ && state_ != State::lost) return false;
    if (held != (staging_ != nullptr)) return false;
    for (uint32_t row = 0u; row < strings; row++)
        if (strings_[row][string_bytes - 1u] != '\0') return false;
    return arrays_.invariants_hold() && uniforms_.invariants_hold() && queries_.invariants_hold() &&
           framebuffers_.invariants_hold() && mappings_.invariants_hold() && shadows_.invariants_hold();
}

Status GuestGlContext::build()
{
    AV_SCOPE("GuestGlContext::build");
    if (connection_.acquire(link_) != Status::ok) return Status::not_supported;
    staging_ = static_cast<uint8_t *>(memory_.allocate(staging_bytes, Pool::paged));
    if (staging_ == nullptr) return Status::no_memory;
    /* Both storages were emptied by teardown or never filled. */
    static_cast<void>(transport_.construct(connection_.device(link_), SubApi::opengl, ring_order, reply_bytes, staging_,
                                           staging_bytes));
    const Status initialized = transport_.object().initialize(options_);
    if (initialized != Status::ok) {
        log_.write("rvkgl: the driver refused the context");
        return initialized == Status::no_memory ? Status::no_memory : Status::invalid_parameter;
    }
    batch_block_ = static_cast<uint8_t *>(memory_.allocate(batch_bytes, Pool::paged));
    if (batch_block_ == nullptr) return Status::no_memory;
    static_cast<void>(batched_.construct(transport_.object(), batch_block_, batch_bytes, staging_bytes));
    static_cast<void>(pacer_.construct(connection_.device(link_).kmt(), log_, connection_.device(link_).adapter(),
                                       connection_.device(link_).device()));
    static_cast<void>(arena_.construct(connection_.device(link_), transport_.object()));
    if (arena_.object().open(arena_bytes) != Status::ok)
        log_.write("rvkgl: no blob arena for the context; bulk data crosses inline");
    return Status::ok;
}

bool GuestGlContext::learn()
{
    AV_SCOPE("GuestGlContext::learn");
    ClientTransport &wire = batched_.object();
    for (uint32_t row = 0u; row < strings; row++) {
        size_t length = 0u;
        if (!glproto::rvk_glGetString(wire, string_names[row], strings_[row], string_bytes - 1u, &length))
            return false;
        strings_[row][length < string_bytes ? length : string_bytes - 1u] = '\0';
    }
    for (uint32_t list = 0u; list < QueryShapes::lists; list++) {
        int32_t length = 0;
        glproto::rvk_glGetIntegerv(wire, QueryShapes::length_queries[list], &length);
        queries_.learn(static_cast<QueryShapes::List>(list), length);
    }
    /* What the host granted, which its version string does not say: on one of
     * this project's two host GPUs the compatibility string is identical to
     * the core one (feature 051, research.md §1). */
    int32_t major = 0, minor = 0, mask = 0;
    glproto::rvk_glGetIntegerv(wire, GrantedProfile::major_query, &major);
    glproto::rvk_glGetIntegerv(wire, GrantedProfile::minor_query, &minor);
    glproto::rvk_glGetIntegerv(wire, GrantedProfile::mask_query, &mask);
    profile_.learn(major < 0 ? 0u : static_cast<uint32_t>(major), minor < 0 ? 0u : static_cast<uint32_t>(minor),
                   static_cast<uint32_t>(mask));
    cap_version();
    policy_.learn(wire, extensions_);
    return !transport_.object().faulted();
}

/* A host newer than the ICD implements is reported as 4.6: the version's
 * leading "major.minor" is rewritten, and the rest of the host's string kept. */
void GuestGlContext::cap_version()
{
    AV_SCOPE("GuestGlContext::cap_version");
    char *const version = strings_[version_row];
    uint32_t    major = 0u;
    uint32_t    minor = 0u;
    uint32_t    at = 0u;
    while (version[at] >= '0' && version[at] <= '9') major = major * 10u + static_cast<uint32_t>(version[at++] - '0');
    if (at == 0u || version[at] != '.') return;
    const uint32_t minor_at = ++at;
    while (version[at] >= '0' && version[at] <= '9') minor = minor * 10u + static_cast<uint32_t>(version[at++] - '0');
    if (at == minor_at || major < latest_major || (major == latest_major && minor <= latest_minor)) return;
    char       capped[string_bytes];
    TextBuffer text(capped, sizeof capped);
    text.append_decimal(latest_major);
    text.append(".");
    text.append_decimal(latest_minor);
    text.append(version + at);
    for (uint32_t byte = 0u; byte <= text.length(); byte++) version[byte] = capped[byte];
}

void GuestGlContext::teardown()
{
    AV_SCOPE("GuestGlContext::teardown");
    /* What the application sent last still reaches the host before its
     * context goes. */
    if (batched_.constructed() && state_ == State::made) static_cast<void>(batched_.object().flush());
    /* The host's framebuffer objects go with its context. */
    framebuffers_.release(nullptr);
    mappings_.release();
    shadows_.forget_all();
    if (arena_.constructed()) {
        /* A lost context's host context goes with DestroyContext, and its
         * import with it: nothing is asked of the host. */
        const Status closed = state_ == State::lost ? arena_.object().abandon() : arena_.object().close();
        if (closed != Status::ok) log_.write("rvkgl: the blob arena did not close whole");
        arena_.destroy();
    }
    batched_.destroy();
    pacer_.destroy();
    transport_.destroy();
    if (staging_ != nullptr) memory_.release(staging_);
    staging_ = nullptr;
    if (batch_block_ != nullptr) memory_.release(batch_block_);
    batch_block_ = nullptr;
    connection_.release(link_);
    link_ = icd::IcdConnection::no_link;
    loss_reported_ = false;
    state_ = State::created;
    owner_ = 0u;
    for (auto &row : strings_) row[0] = '\0';
}

}  // namespace av::glicd
