#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "icd/opengl/gl_debug_messages.hpp"
#include "icd/opengl/gl_extension_policy.hpp"
#include "icd/opengl/guest_gl_context.hpp"

#include "gl_dispatch_client.h"

#include <cstdint>
#include <cstring>

using av::GlContextOptions;
using av::GlContextRig;
using av::Status;
using av::glicd::GlDebugMessages;
using av::glicd::GlExtensionPolicy;
using av::glicd::GuestGlContext;

namespace {

struct Seen {
    std::uint32_t count = 0u;
    GLenum        source = 0u;
    GLenum        type = 0u;
    GLuint        id = 0u;
    GLenum        severity = 0u;
    GLsizei       length = 0;
    char          text[64] = {};
    const void   *user = nullptr;
};

void APIENTRY record(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message,
                     const void *user)
{
    Seen *const seen = const_cast<Seen *>(static_cast<const Seen *>(user));
    seen->count++;
    seen->source = source;
    seen->type = type;
    seen->id = id;
    seen->severity = severity;
    seen->length = length;
    std::strncpy(seen->text, message, sizeof seen->text - 1u);
    seen->user = user;
}

void put_varint(std::uint8_t *&at, std::uint32_t value)
{
    *at++ = static_cast<std::uint8_t>(value);
}

void put_word(std::uint8_t *&at, std::uint32_t value)
{
    put_varint(at, 4u);
    std::memcpy(at, &value, 4u);
    at += 4;
}

std::uint32_t one_message(std::uint8_t *out, std::uint32_t id, const char *text)
{
    std::uint8_t *at = out;
    put_varint(at, 1u);
    put_word(at, 0x8249u);
    put_word(at, 0x824Cu);
    put_word(at, id);
    put_word(at, 0x9146u);
    const std::uint32_t length = static_cast<std::uint32_t>(std::strlen(text)) + 1u;
    put_word(at, length);
    put_varint(at, length);
    std::memcpy(at, text, length);
    at += length;
    return static_cast<std::uint32_t>(at - out);
}

struct Made {
    GlContextRig      rig;
    GlExtensionPolicy empty{nullptr, 0u, nullptr, 0u};
    GuestGlContext    context{rig.connection(), rig.platform(), rig.send_lock(), rig.groups(), rig.lines(), empty, rig.handles(), 1,
                              GlContextOptions()};

    Made()
    {
        rig.host().gl_made("host gpu", "4.6.0 host");
        CHECK_STATUS(context.make(), Status::ok);
    }
    void answer_message(std::uint32_t id, const char *text)
    {
        std::uint8_t bytes[128];
        rig.host().gl_raw(bytes, one_message(bytes, id, text));
    }
    void answer_empty()
    {
        const std::uint8_t none[] = {0u};
        rig.host().gl_raw(none, sizeof none);
    }
};

}

TEST(gl_debug_messages, only_the_two_debug_capabilities_are_tracked)
{ GlDebugMessages messages(false);
  CHECK(messages.tracks(GlDebugMessages::debug_output));
  CHECK(messages.tracks(GlDebugMessages::debug_output_synchronous));
  CHECK(!messages.tracks(0x0B71u));
  CHECK(!messages.listening());
  messages.set_callback(0x1000u, 0x2000u);
  CHECK(!messages.listening());
  messages.enable(GlDebugMessages::debug_output, true);
  CHECK(messages.listening());
  CHECK(!messages.synchronous());
  messages.enable(GlDebugMessages::debug_output_synchronous, true);
  CHECK(messages.synchronous());
  messages.enable(GlDebugMessages::debug_output, false);
  CHECK(!messages.synchronous());
  CHECK(messages.invariants_hold()); }

TEST(gl_debug_messages, a_debug_context_starts_with_debug_output_on_and_synchronous_off)
{ GlDebugMessages messages(true);
  messages.set_callback(0x1000u, 0u);
  CHECK(messages.listening());
  CHECK(!messages.synchronous()); }

TEST(gl_debug_messages, every_logged_host_message_reaches_the_callback_without_its_terminator)
{ Made made;
  Seen seen;
  made.context.debug().set_callback(reinterpret_cast<std::uint64_t>(&record), reinterpret_cast<std::uint64_t>(&seen));
  made.context.debug().enable(GlDebugMessages::debug_output, true);
  made.answer_message(4711u, "marker");
  made.answer_message(4712u, "second");
  made.answer_empty();
  made.context.deliver_debug();
  CHECK_EQ(seen.count, 2u);
  CHECK_EQ(seen.source, 0x8249u);
  CHECK_EQ(seen.type, 0x824Cu);
  CHECK_EQ(seen.id, 4712u);
  CHECK_EQ(seen.severity, 0x9146u);
  CHECK_EQ(seen.length, 6);
  CHECK(std::strcmp(seen.text, "second") == 0);
  CHECK(seen.user == &seen);
  CHECK_EQ(made.rig.kmt().queued_answers(), 0u); }

TEST(gl_debug_messages, no_callback_or_output_off_asks_the_host_nothing)
{ Made made;
  const std::uint32_t before = made.rig.kmt().commands();
  made.context.deliver_debug();
  made.context.debug().set_callback(0x1000u, 0u);
  made.context.deliver_debug();
  CHECK_EQ(made.rig.kmt().commands(), before); }

TEST(gl_debug_messages, in_synchronous_mode_every_call_hands_over_its_messages_before_it_returns)
{ Made made;
  Seen seen;
  made.context.debug().set_callback(reinterpret_cast<std::uint64_t>(&record), reinterpret_cast<std::uint64_t>(&seen));
  made.context.debug().enable(GlDebugMessages::debug_output, true);
  made.context.debug().enable(GlDebugMessages::debug_output_synchronous, true);
  av::ClientTransport *const wire = made.context.wire();
  CHECK(wire != nullptr);
  made.answer_message(7u, "invalid enum");
  made.answer_empty();
  av::glproto::av_glEnable(*wire, 0xFFFFu);
  CHECK_EQ(seen.count, 1u);
  CHECK_EQ(seen.id, 7u);
  CHECK_EQ(made.rig.kmt().queued_answers(), 0u);
  made.context.debug().enable(GlDebugMessages::debug_output_synchronous, false);
  const std::uint32_t before = made.rig.kmt().commands();
  av::glproto::av_glEnable(*made.context.wire(), 0xFFFFu);
  CHECK_EQ(made.rig.kmt().commands() - before, 0u);
  CHECK_EQ(seen.count, 1u); }
