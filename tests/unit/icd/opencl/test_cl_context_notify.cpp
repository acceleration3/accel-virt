#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_context_notify.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::clicd::ClContextNotify;

namespace {

struct Heard {
    std::uint32_t times = 0u;
    const char   *text = nullptr;
    const void   *info = reinterpret_cast<const void *>(1);
    std::size_t   bytes = 99u;
};

void heard(const char *text, const void *info, std::size_t bytes, void *user_data)
{
    Heard *const record = static_cast<Heard *>(user_data);
    record->times++;
    record->text = text;
    record->info = info;
    record->bytes = bytes;
}

}

TEST(cl_context_notify, says_the_text_to_the_callback_with_its_own_data)
{ Heard           record;
  ClContextNotify notify(&heard, &record);
  notify.say("lost");
  CHECK_EQ(record.times, 1u);
  CHECK(std::strcmp(record.text, "lost") == 0);
  CHECK(record.info == nullptr);
  CHECK_EQ(record.bytes, 0u); }

TEST(cl_context_notify, none_registered_says_nothing)
{ ClContextNotify none;
  none.say("lost");
  Heard           record;
  ClContextNotify unregistered(nullptr, &record);
  unregistered.say("lost");
  CHECK_EQ(record.times, 0u); }
