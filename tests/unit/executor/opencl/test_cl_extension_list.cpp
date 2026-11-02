#include "executor/opencl/cl_extension_list.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstring>

namespace av {

struct ClExtensionListProbe {
    static void set(ClExtensionList &list, const char *text, std::size_t length)
    {
        std::memcpy(list.text_, text, length);
        list.length_ = length;
    }
    static void length(ClExtensionList &list, std::size_t length) { list.length_ = length; }
};

}

using av::ClExtensionList;
using av::ClExtensionListProbe;

namespace {
const char *shown(const ClExtensionList &list)
{
    static char text[ClExtensionList::host_bytes + ClExtensionList::added_bytes];
    std::memset(text, 'x', sizeof text);
    return list.answer(sizeof text, text, nullptr) == CL_SUCCESS ? text : "";
}

}

TEST(cl_extension_list, the_host_s_words_are_kept_one_space_apart)
{ ClExtensionList list;
  CHECK(list.invariants_hold());
  CHECK(std::strcmp(shown(list), "") == 0);
  const char host[] = "  cl_khr_fp64   cl_khr_gl_sharing cl_khr_icd  ";
  list.hold(host, sizeof host);
  CHECK(list.invariants_hold());
  CHECK(std::strcmp(shown(list), "cl_khr_fp64 cl_khr_gl_sharing cl_khr_icd") == 0);
  list.hold(host, 13u);
  CHECK(std::strcmp(shown(list), "cl_khr_fp64") == 0);
  list.hold(host, 0u);
  CHECK(std::strcmp(shown(list), "") == 0);
  CHECK(list.invariants_hold()); }

TEST(cl_extension_list, a_host_list_longer_than_the_room_is_cut_at_it)
{ static char host[ClExtensionList::host_bytes * 2u];
  std::memset(host, 'a', sizeof host - 1u);
  host[sizeof host - 1u] = '\0';
  ClExtensionList list;
  list.hold(host, sizeof host);
  CHECK(list.invariants_hold());
  std::size_t needed = 0u;
  CHECK_EQ(list.answer(0u, nullptr, &needed), CL_SUCCESS);
  CHECK_EQ(needed, ClExtensionList::host_bytes); }

TEST(cl_extension_list, a_struck_word_goes_and_a_word_it_begins_or_ends_stays)
{ ClExtensionList list;
  const char host[] = "cl_khr_gl_sharing_x cl_khr_gl_sharing x_cl_khr_gl_sharing cl_khr_gl_sharing";
  list.hold(host, sizeof host);
  CHECK(list.names("cl_khr_gl_sharing"));
  CHECK(!list.names("cl_khr_gl"));
  list.strike("cl_khr_gl_sharing");
  CHECK(list.invariants_hold());
  CHECK(!list.names("cl_khr_gl_sharing"));
  CHECK(std::strcmp(shown(list), "cl_khr_gl_sharing_x x_cl_khr_gl_sharing") == 0);
  list.strike("cl_khr_gl_sharing_x");
  CHECK(std::strcmp(shown(list), "x_cl_khr_gl_sharing") == 0);
  list.strike("x_cl_khr_gl_sharing");
  CHECK(std::strcmp(shown(list), "") == 0);
  CHECK(list.invariants_hold());
  list.hold(host, sizeof host);
  list.strike("cl_khr_fp64");
  CHECK(std::strcmp(shown(list), host) == 0); }

TEST(cl_extension_list, a_word_is_added_once_at_the_end_and_only_where_it_fits)
{ ClExtensionList list;
  list.add("cl_khr_external_memory");
  CHECK(std::strcmp(shown(list), "cl_khr_external_memory") == 0);
  const char host[] = "cl_khr_fp64 cl_khr_external_memory_opaque_fd";
  list.hold(host, sizeof host);
  list.add("cl_khr_external_memory");
  list.add("cl_khr_external_memory");
  CHECK(list.invariants_hold());
  CHECK(std::strcmp(shown(list), "cl_khr_fp64 cl_khr_external_memory_opaque_fd cl_khr_external_memory") == 0);
  list.add("");
  CHECK(std::strcmp(shown(list), "cl_khr_fp64 cl_khr_external_memory_opaque_fd cl_khr_external_memory") == 0);
  static char full[ClExtensionList::host_bytes];
  std::memset(full, 'a', sizeof full - 1u);
  full[sizeof full - 1u] = '\0';
  list.hold(full, sizeof full);
  static char long_name[ClExtensionList::added_bytes + 1u];
  std::memset(long_name, 'b', sizeof long_name - 1u);
  long_name[sizeof long_name - 1u] = '\0';
  list.add(long_name);
  CHECK(!list.names(long_name));
  long_name[ClExtensionList::added_bytes - 2u] = '\0';
  list.add(long_name);
  CHECK(list.names(long_name));
  CHECK(list.invariants_hold()); }

TEST(cl_extension_list, the_answer_is_clGetDeviceInfo_s)
{ ClExtensionList list;
  const char host[] = "cl_khr_fp64 cl_khr_icd";
  list.hold(host, sizeof host);
  std::size_t needed = 0u;
  CHECK_EQ(list.answer(0u, nullptr, &needed), CL_SUCCESS);
  CHECK_EQ(needed, sizeof host);
  char exact[sizeof host] = {};
  CHECK_EQ(list.answer(sizeof exact, exact, nullptr), CL_SUCCESS);
  CHECK(std::strcmp(exact, host) == 0);
  char cramped[sizeof host - 1u];
  std::memset(cramped, 'x', sizeof cramped);
  needed = 0u;
  CHECK_EQ(list.answer(sizeof cramped, cramped, &needed), CL_INVALID_VALUE);
  CHECK_EQ(cramped[0], 'x');
  CHECK_EQ(needed, sizeof host); }

TEST(cl_extension_list, the_invariant_fails_each_way_the_list_can_be_wrong)
{ struct Case {
      const char *text;
      std::size_t length;
  };
  const Case cases[] = {{"abcx", 3u}, {" ab\0", 3u}, {"ab \0", 3u}, {"a\0b\0", 3u}, {"a  b\0", 4u}};
  for (const Case &item : cases) {
      ClExtensionList list;
      ClExtensionListProbe::set(list, item.text, item.length + 1u);
      ClExtensionListProbe::length(list, item.length);
      CHECK(!list.invariants_hold());
  }
  ClExtensionList list;
  ClExtensionListProbe::length(list, ClExtensionList::host_bytes + ClExtensionList::added_bytes);
  CHECK(!list.invariants_hold()); }
