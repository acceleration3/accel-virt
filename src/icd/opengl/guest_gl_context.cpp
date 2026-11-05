#include "icd/opengl/guest_gl_context.hpp"

#include "icd/common/version_text.hpp"

#include "core/platform/block_source.hpp"
#include "core/platform/thread_lock.hpp"
#include "core/support/scope_exit.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/log/keyed_log.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/common/icd_connection.hpp"
#include "icd/opengl/gl_extension_policy.hpp"
#include "proto/client/sync_call_scope.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/command_header.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

namespace {

constexpr uint32_t latest_major = 4u;
constexpr uint32_t latest_minor = 6u;
constexpr uint32_t latest_language_minor = 60u;
constexpr uint32_t no_error = 0u;

}

GuestGlContext::GuestGlContext(icd::IcdConnection &connection, BlockSource &memory, ThreadLock &send_lock,
                               KeyedLog &lines, const GlExtensionPolicy &extensions,
                               icd::SharedHandles &handles, int32_t pixel_format,
                               const GlContextOptions &options)
    : memory_(memory), lines_(lines), policy_(extensions),
      pixel_format_(pixel_format),
      options_(options), state_(State::created), owner_(0u), current_dc_(nullptr), been_current_(false), error_(no_error), share_target_(0u), link_(connection), loss_(lines), loss_reported_(false),
      channel_(memory, lines, icd::ChannelShape(SubApi::opengl, ring_order, reply_bytes, staging_bytes, batch_bytes, arena_bytes), send_lock), reported_(), arrays_(memory), uniforms_(memory), queries_(), profile_(), pacing_(), frames_(), timeline_(channel_, send_lock), extensions_(memory), framebuffers_(memory, lines), shadows_(memory), staging_(), mappings_(memory, shadows_, lines), externals_(handles, memory, lines), pinned_(memory, channel_),
      strings_{},
      debug_callback_(0u), debug_user_(0u), luid_(0u)
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
        lines_.write("the host refused the context's options");
        return Status::invalid_parameter;
    }
    undo.dismiss();
    state_ = State::made;
    return Status::ok;
}

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
    if (dc != nullptr) been_current_ = true;
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
    return state_ == State::created ? 0u : channel_.transport().ring();
}

Status GuestGlContext::share_after(uint64_t target)
{
    AV_SCOPE("GuestGlContext::share_after");
    if (share_target_ != 0u || been_current_ || state_ == State::lost) return Status::invalid_state;
    if (state_ == State::made) teardown();
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

BatchMark GuestGlContext::mark() const
{
    AV_SCOPE("GuestGlContext::mark");
    return channel_.opened() ? channel_.transport().newest_mark() : BatchMark();
}

void GuestGlContext::flush()
{
    AV_SCOPE("GuestGlContext::flush");
    if (state_ != State::made) return;
    const TransportResult sent = channel_.flush();
    if (sent != TransportResult::ok) lines_.write("the held calls could not be sent");
}

Status GuestGlContext::run_held()
{
    AV_SCOPE("GuestGlContext::run_held");
    if (state_ != State::made) return Status::invalid_state;
    if (channel_.run_held() == Status::ok) return Status::ok;
    lines_.write("the held calls could not be run for another API");
    return Status::invalid_state;
}

uint32_t GuestGlContext::guest_device() const
{
    AV_SCOPE("GuestGlContext::guest_device");
    if (!link_.held()) return 0u;
    return link_.device().device();
}

void GuestGlContext::end_frame()
{
    AV_SCOPE("GuestGlContext::end_frame");
    if (state_ != State::made) return;
    flush();
    uint64_t       frequency = 0u;
    const uint64_t now = link_.device().kmt().performance_counter(frequency);
    const uint64_t behind = frames_.end(timeline_.recording() - 1u, now, frequency, timeline_.completed());
    if (behind != 0u && !timeline_.wait(behind))
        lines_.write("the host never finished an earlier frame; the context is lost");
}

ClientTransport *GuestGlContext::wire()
{
    AV_SCOPE("GuestGlContext::wire");
    if (state_ != State::made) return nullptr;
    if (link_.lost(channel_)) {
        state_ = State::lost;
        loss_.lose(channel_.fault_code());
        static_cast<void>(loss_.refuses("an OpenGL call"));
    }
    return state_ == State::made ? &channel_.transport() : nullptr;
}

ClientArrayState &GuestGlContext::arrays()
{
    AV_SCOPE("GuestGlContext::arrays");
    return arrays_;
}

PinnedBuffers &GuestGlContext::pinned()
{
    AV_SCOPE("GuestGlContext::pinned");
    return pinned_;
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

PixelStagingBuffer &GuestGlContext::staging()
{
    AV_SCOPE("GuestGlContext::staging");
    return staging_;
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

bool GuestGlContext::take_host_error(uint32_t &error)
{
    AV_SCOPE("GuestGlContext::take_host_error");
    error = no_error;
    if (channel_.arena_view() == nullptr) return false;
    const uint64_t word = channel_.host_errors();
    if (!reported_.readable(word)) return false;
    error = reported_.take(word);
    return true;
}

const char *GuestGlContext::string(uint32_t name) const
{
    AV_SCOPE("GuestGlContext::string");
    if (state_ == State::created) return nullptr;
    for (uint32_t row = 0u; row < strings; row++)
        if (string_names[row] == name) return strings_[row];
    return nullptr;
}

icd::PresentPacing &GuestGlContext::pacing()
{
    AV_SCOPE("GuestGlContext::pacing");
    return pacing_;
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
    if (state_ != State::lost || !robust()) return no_error;
    return loss_.guilty() ? gl_guilty_context_reset : gl_unknown_context_reset;
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
    writer.number("device", guest_device());
    loss_.report(writer);
    writer.flag("arena", channel_.arena_view() != nullptr);
    writer.text("renderer", state_ == State::created ? "" : strings_[renderer_row]);
    writer.number("extensions", extensions_.count());
    framebuffers_.report(writer);
    mappings_.report(writer);
    shadows_.report(writer);
    writer.close();
}

bool GuestGlContext::invariants_hold() const
{
    AV_SCOPE("GuestGlContext::invariants_hold");
    const bool held = state_ != State::created;
    if (held != channel_.opened() || held != link_.held()) return false;
    if (loss_reported_ && state_ != State::lost) return false;
    if (loss_.lost() != (state_ == State::lost) || !loss_.invariants_hold()) return false;
    if (!held && staging_.name() != 0u) return false;
    for (uint32_t row = 0u; row < strings; row++)
        if (strings_[row][string_bytes - 1u] != '\0') return false;
    return arrays_.invariants_hold() && uniforms_.invariants_hold() && queries_.invariants_hold() &&
           framebuffers_.invariants_hold() && mappings_.invariants_hold() && shadows_.invariants_hold() &&
           staging_.invariants_hold() && channel_.invariants_hold() && pacing_.invariants_hold();
}

Status GuestGlContext::build()
{
    AV_SCOPE("GuestGlContext::build");
    if (link_.acquire() != Status::ok) return Status::not_supported;
    GuestConnection &device = link_.device();
    const Status     opened = channel_.open(device, options_);
    if (opened != Status::ok) return opened == Status::no_memory ? Status::no_memory : Status::invalid_parameter;
    static_cast<void>(pacing_.bind(device.kmt(), lines_, device.adapter(), device.device()));
    if (device.kmt().adapter_luid(device.adapter(), luid_) != Status::ok) luid_ = 0u;
    return Status::ok;
}

uint64_t GuestGlContext::adapter_luid() const
{
    AV_SCOPE("GuestGlContext::adapter_luid");
    return luid_;
}

bool GuestGlContext::learn()
{
    AV_SCOPE("GuestGlContext::learn");
    ClientTransport &wire = channel_.transport();
    for (uint32_t row = 0u; row < strings; row++) {
        size_t length = 0u;
        if (!glproto::av_glGetString(wire, string_names[row], strings_[row], string_bytes - 1u, &length))
            return false;
        strings_[row][length < string_bytes ? length : string_bytes - 1u] = '\0';
    }
    for (uint32_t list = 0u; list < QueryShapes::lists; list++) {
        int32_t length = 0;
        glproto::av_glGetIntegerv(wire, QueryShapes::length_queries[list], &length);
        queries_.learn(static_cast<QueryShapes::List>(list), length);
    }
    int32_t major = 0, minor = 0, mask = 0;
    glproto::av_glGetIntegerv(wire, GrantedProfile::major_query, &major);
    glproto::av_glGetIntegerv(wire, GrantedProfile::minor_query, &minor);
    glproto::av_glGetIntegerv(wire, GrantedProfile::mask_query, &mask);
    profile_.learn(major < 0 ? 0u : static_cast<uint32_t>(major), minor < 0 ? 0u : static_cast<uint32_t>(minor),
                   static_cast<uint32_t>(mask));
    cap_version(version_row, latest_minor);
    cap_version(shading_row, latest_language_minor);
    policy_.learn(wire, extensions_);
    return !channel_.lost();
}

void GuestGlContext::cap_version(uint32_t row, uint32_t most_minor)
{
    AV_SCOPE("GuestGlContext::cap_version");
    icd::VersionText version("", latest_major, most_minor);
    char *const      text = strings_[row];
    const char      *rest = nullptr;
    if (version.read(text, string_bytes, rest) != Status::ok) return;
    char capped[string_bytes];
    version.write(capped, sizeof capped, rest);
    for (uint32_t byte = 0u; byte < string_bytes; byte++) {
        text[byte] = capped[byte];
        if (capped[byte] == '\0') return;
    }
}

void GuestGlContext::teardown()
{
    AV_SCOPE("GuestGlContext::teardown");
    framebuffers_.release(nullptr);
    mappings_.release();
    shadows_.forget_all();
    if (state_ == State::made)
        staging_.release(channel_.transport());
    else
        staging_.forget();
    channel_.close(state_ == State::lost);
    pacing_.unbind();
    frames_.forget();
    timeline_.forget();
    link_.release();
    loss_reported_ = false;
    loss_.forget();
    state_ = State::created;
    owner_ = 0u;
    for (auto &row : strings_) row[0] = '\0';
}

}
