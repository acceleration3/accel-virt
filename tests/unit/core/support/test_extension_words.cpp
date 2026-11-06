#include "core/support/extension_words.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstddef>
#include <cstring>

#include <sys/mman.h>
#include <unistd.h>

namespace av {

struct GrowableArrayProbe {
    template <typename T>
    static void corrupt_size(GrowableArray<T> &array, std::uint32_t size) { array.size_ = size; }
};

struct ExtensionWordsProbe {
    static void corrupt_size(ExtensionWords &words, std::uint32_t size) { GrowableArrayProbe::corrupt_size(words.text_, size); }
    static void set(ExtensionWords &words, const char *text, std::size_t bytes)
    {
        words.text_.truncate(0u);
        for (std::size_t at = 0u; at < bytes; at++) static_cast<void>(words.text_.append(text[at]));
    }
};

}

using av::ExtensionWords;
using av::ExtensionWordsProbe;
using av::MockPlatform;
using av::Pool;
using av::Status;

namespace {

class VastText {
public:
    static constexpr std::size_t letters = std::size_t{1} << 31u;

    VastText() : base_(nullptr)
    {
        constexpr std::size_t chunk = std::size_t{2} << 20u;
        const int             file = memfd_create("vast_text", 0);
        CHECK(file >= 0);
        CHECK_EQ(ftruncate(file, static_cast<off_t>(chunk)), 0);
        void *const page = mmap(nullptr, chunk, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
        CHECK(page != MAP_FAILED);
        std::memset(page, 'a', chunk);
        munmap(page, chunk);
        void *const whole = mmap(nullptr, letters + chunk, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        CHECK(whole != MAP_FAILED);
        base_ = static_cast<char *>(whole);
        for (std::size_t at = 0u; at < letters; at += chunk)
            CHECK(mmap(base_ + at, chunk, PROT_READ, MAP_SHARED | MAP_FIXED, file, 0) != MAP_FAILED);
        close(file);
    }
    ~VastText() { munmap(base_, letters + (std::size_t{2} << 20u)); }
    VastText(const VastText &) = delete;
    VastText &operator=(const VastText &) = delete;

    const char *text() const { return base_; }

private:
    char *base_;
};

}

TEST(extension_words, the_host_s_words_are_kept_one_space_apart)
{ MockPlatform platform;
  ExtensionWords words(platform, Pool::paged);
  CHECK(words.invariants_hold());
  CHECK(std::strcmp(words.text(), "") == 0);
  CHECK_EQ(words.length(), 0u);
  const char host[] = "  cl_khr_fp64   cl_khr_gl_sharing cl_khr_icd  ";
  CHECK_STATUS(words.hold(host, sizeof host), Status::ok);
  CHECK(words.invariants_hold());
  CHECK(std::strcmp(words.text(), "cl_khr_fp64 cl_khr_gl_sharing cl_khr_icd") == 0);
  CHECK_EQ(words.length(), std::strlen("cl_khr_fp64 cl_khr_gl_sharing cl_khr_icd"));
  CHECK_STATUS(words.hold(host, 13u), Status::ok);
  CHECK(std::strcmp(words.text(), "cl_khr_fp64") == 0);
  CHECK_STATUS(words.hold(host, 0u), Status::ok);
  CHECK(std::strcmp(words.text(), "") == 0);
  CHECK_STATUS(words.hold(nullptr, 5u), Status::ok);
  CHECK_EQ(words.length(), 0u);
  CHECK(words.invariants_hold()); }

TEST(extension_words, a_struck_word_goes_and_a_word_it_begins_or_ends_stays)
{ MockPlatform platform;
  ExtensionWords words(platform, Pool::paged);
  const char host[] = "cl_khr_gl_sharing_x cl_khr_gl_sharing x_cl_khr_gl_sharing cl_khr_gl_sharing";
  CHECK_STATUS(words.hold(host, sizeof host), Status::ok);
  CHECK(words.holds("cl_khr_gl_sharing"));
  CHECK(!words.holds("cl_khr_gl"));
  CHECK(!words.holds(""));
  CHECK(!words.holds(nullptr));
  words.strike("cl_khr_gl_sharing");
  CHECK(words.invariants_hold());
  CHECK(!words.holds("cl_khr_gl_sharing"));
  CHECK(std::strcmp(words.text(), "cl_khr_gl_sharing_x x_cl_khr_gl_sharing") == 0);
  words.strike("cl_khr_gl_sharing_x");
  CHECK(std::strcmp(words.text(), "x_cl_khr_gl_sharing") == 0);
  words.strike("x_cl_khr_gl_sharing");
  CHECK(std::strcmp(words.text(), "") == 0);
  CHECK(words.invariants_hold());
  CHECK_STATUS(words.hold(host, sizeof host), Status::ok);
  words.strike("cl_khr_fp64");
  CHECK(std::strcmp(words.text(), host) == 0);
  words.strike(nullptr);
  CHECK(std::strcmp(words.text(), host) == 0); }

TEST(extension_words, a_word_is_added_once_at_the_end)
{ MockPlatform platform;
  ExtensionWords words(platform, Pool::paged);
  CHECK_STATUS(words.add("cl_khr_external_memory"), Status::ok);
  CHECK(std::strcmp(words.text(), "cl_khr_external_memory") == 0);
  const char host[] = "cl_khr_fp64 cl_khr_external_memory_opaque_fd";
  CHECK_STATUS(words.hold(host, sizeof host), Status::ok);
  CHECK_STATUS(words.add("cl_khr_external_memory"), Status::ok);
  CHECK_STATUS(words.add("cl_khr_external_memory"), Status::ok);
  CHECK(words.invariants_hold());
  CHECK(std::strcmp(words.text(), "cl_khr_fp64 cl_khr_external_memory_opaque_fd cl_khr_external_memory") == 0);
  CHECK_STATUS(words.add(""), Status::invalid_parameter);
  CHECK_STATUS(words.add(nullptr), Status::invalid_parameter);
  CHECK_STATUS(words.add("two words"), Status::invalid_parameter);
  CHECK(std::strcmp(words.text(), "cl_khr_fp64 cl_khr_external_memory_opaque_fd cl_khr_external_memory") == 0);
  CHECK(words.invariants_hold()); }

TEST(extension_words, a_list_grows_past_a_block_and_clear_empties_it)
{ MockPlatform platform;
  ExtensionWords words(platform, Pool::non_paged);
  static char host[20000];
  std::memset(host, 'a', sizeof host - 1u);
  host[sizeof host - 1u] = '\0';
  CHECK_STATUS(words.hold(host, sizeof host), Status::ok);
  CHECK_EQ(words.length(), sizeof host - 1u);
  CHECK_STATUS(words.add("cl_khr_icd"), Status::ok);
  CHECK(words.holds("cl_khr_icd"));
  CHECK(words.invariants_hold());
  words.clear();
  CHECK_EQ(words.length(), 0u);
  CHECK(std::strcmp(words.text(), "") == 0);
  CHECK(words.invariants_hold()); }

TEST(extension_words, the_invariant_fails_each_way_the_text_can_be_wrong)
{ struct Case {
      const char *text;
      std::size_t bytes;
  };
  const Case cases[] = {{"abx", 3u}, {" ab\0", 4u}, {"ab \0", 4u}, {"a\0b\0", 4u}, {"a  b\0", 5u}};
  for (const Case &item : cases) {
      MockPlatform  platform;
      ExtensionWords words(platform, Pool::paged);
      ExtensionWordsProbe::set(words, item.text, item.bytes);
      CHECK(!words.invariants_hold());
  } }

TEST(extension_words, striking_from_an_empty_list_changes_nothing)
{ MockPlatform platform;
  ExtensionWords words(platform, Pool::paged);
  words.strike("cl_khr_icd");
  CHECK_EQ(words.length(), 0u);
  CHECK(std::strcmp(words.text(), "") == 0);
  CHECK(words.invariants_hold()); }

TEST(extension_words, a_host_string_the_array_cannot_reserve_leaves_the_list_empty)
{ MockPlatform platform;
  ExtensionWords words(platform, Pool::paged);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(words.hold("cl_khr_icd", 11u), Status::no_memory);
  CHECK_EQ(words.length(), 0u);
  CHECK(words.invariants_hold());
  CHECK_STATUS(words.add("cl_khr_fp64"), Status::ok);
  CHECK(words.invariants_hold()); }

TEST(extension_words, a_short_host_string_is_kept_whatever_the_stated_size)
{ MockPlatform platform;
  ExtensionWords words(platform, Pool::paged);
  CHECK_STATUS(words.hold("cl_khr_icd", ~std::size_t{0}), Status::ok);
  CHECK(std::strcmp(words.text(), "cl_khr_icd") == 0);
  CHECK(words.invariants_hold()); }

TEST(extension_words, a_word_of_two_billion_letters_is_refused_as_too_big)
{ MockPlatform platform;
  ExtensionWords words(platform, Pool::paged);
  const VastText vast;
  CHECK_STATUS(words.add(vast.text()), Status::no_memory);
  CHECK_EQ(words.length(), 0u);
  CHECK(words.invariants_hold()); }

TEST(extension_words, a_broken_text_array_breaks_the_invariant)
{ MockPlatform platform;
  ExtensionWords words(platform, Pool::paged);
  CHECK_STATUS(words.add("cl_khr_icd"), Status::ok);
  CHECK(words.invariants_hold());
  ExtensionWordsProbe::corrupt_size(words, 99u);
  CHECK(!words.invariants_hold());
  ExtensionWordsProbe::corrupt_size(words, 11u); }
