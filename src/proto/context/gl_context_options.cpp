#include "proto/context/gl_context_options.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {

/* The highest minor each major version of OpenGL has, from 1 to 4. */
constexpr uint8_t highest_minor[] = {5u, 1u, 3u, 6u};
constexpr uint8_t highest_major = sizeof highest_minor / sizeof highest_minor[0];

}  // namespace

GlContextOptions::GlContextOptions() : major_(0u), minor_(0u), profile_(0u), flags_(0u), share_context_(0u)
{
    AV_SCOPE("GlContextOptions::GlContextOptions");
}

GlContextOptions::GlContextOptions(uint8_t major, uint8_t minor, Profile profile, uint8_t flags,
                                   uint32_t share_context)
    : major_(major), minor_(minor), profile_(static_cast<uint8_t>(profile)), flags_(flags),
      share_context_(share_context)
      {
          AV_SCOPE("GlContextOptions::GlContextOptions");
      }

uint8_t                  GlContextOptions::major() const
{
    AV_SCOPE("GlContextOptions::major");
    return major_;
}
uint8_t                  GlContextOptions::minor() const
{
    AV_SCOPE("GlContextOptions::minor");
    return minor_;
}
GlContextOptions::Profile GlContextOptions::profile() const
{
    AV_SCOPE("GlContextOptions::profile");
    return static_cast<Profile>(profile_);
}
uint8_t                  GlContextOptions::flags() const
{
    AV_SCOPE("GlContextOptions::flags");
    return flags_;
}
uint32_t                 GlContextOptions::share_context() const
{
    AV_SCOPE("GlContextOptions::share_context");
    return share_context_;
}

GlContextOptions GlContextOptions::sharing(uint32_t share_context) const
{
    AV_SCOPE("GlContextOptions::sharing");
    GlContextOptions shared = *this;
    shared.share_context_ = share_context;
    return shared;
}

bool GlContextOptions::defaults() const
{
    AV_SCOPE("GlContextOptions::defaults");
    return major_ == 0u && minor_ == 0u && profile_ == 0u && flags_ == 0u && share_context_ == 0u;
}

bool GlContextOptions::shares_only() const
{
    AV_SCOPE("GlContextOptions::shares_only");
    return major_ == 0u && minor_ == 0u && profile_ == 0u && flags_ == 0u;
}

bool GlContextOptions::valid() const
{
    AV_SCOPE("GlContextOptions::valid");
    if (major_ == 0u) return defaults();
    return major_ <= highest_major && minor_ <= highest_minor[major_ - 1u] &&
           profile_ <= static_cast<uint8_t>(Profile::compatibility) && (flags_ & ~defined_flags) == 0u;
}

void GlContextOptions::write(WireWriter &writer) const
{
    AV_SCOPE("GlContextOptions::write");
    writer.write_u8(major_);
    writer.write_u8(minor_);
    writer.write_u8(profile_);
    writer.write_u8(flags_);
    writer.write_u32(share_context_);
}

void GlContextOptions::read(WireReader &reader)
{
    AV_SCOPE("GlContextOptions::read");
    major_ = reader.read_u8();
    minor_ = reader.read_u8();
    profile_ = reader.read_u8();
    flags_ = reader.read_u8();
    share_context_ = reader.read_u32();
}

}  // namespace av
