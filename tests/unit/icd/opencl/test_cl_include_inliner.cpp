#include "harness/framework/test_registry.hpp"
#include "harness/icd/scripted_header_source.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_include_inliner.hpp"
#include "icd/opencl/cl_program_source.hpp"

#include <cstring>

using av::MockPlatform;
using av::ScriptedHeaderSource;
using av::clicd::ClIncludeInliner;
using av::clicd::ClProgramSource;

TEST(cl_include_inliner, include_directories_are_taken_out_of_the_options_the_host_gets)
{ MockPlatform platform;
  ScriptedHeaderSource headers;
  ClIncludeInliner inliner(platform, headers);
  CHECK(inliner.read_options("-D A=1 -I C:\\inc -IC:\\more -cl-std=CL1.2") == av::Status::ok);
  CHECK_EQ(inliner.directories(), 2u);
  CHECK_EQ(std::strcmp(inliner.host_options(), "-D A=1 -cl-std=CL1.2"), 0);
  ClIncludeInliner none(platform, headers);
  CHECK(none.read_options(nullptr) == av::Status::ok);
  CHECK_EQ(none.directories(), 0u);
  CHECK_EQ(std::strcmp(none.host_options(), ""), 0); }

TEST(cl_include_inliner, an_include_is_replaced_by_the_file_found_in_the_first_directory_that_has_it)
{ MockPlatform platform;
  ScriptedHeaderSource headers;
  headers.add("D:\\two", "a.h", "#define A 1\n#include \"b.h\"");
  headers.add("D:\\two", "b.h", "#define B 2\n");
  ClIncludeInliner inliner(platform, headers);
  CHECK(inliner.read_options("-I D:\\one -I D:\\two") == av::Status::ok);
  const char source[] = "#include \"a.h\"\nint x;\n#include <missing.h>\n";
  CHECK(inliner.expand(source, sizeof source - 1u) == av::Status::ok);
  CHECK_EQ(std::strcmp(inliner.text(), "#define A 1\n#define B 2\nint x;\n#include <missing.h>\n"), 0);
  CHECK_EQ(inliner.text_bytes(), std::strlen(inliner.text())); }

TEST(cl_include_inliner, a_header_marked_once_and_a_cycle_are_read_once)
{ MockPlatform platform;
  ScriptedHeaderSource headers;
  headers.add("d", "once.h", "#pragma once\nint o;\n");
  headers.add("d", "loop.h", "int l;\n#include \"loop.h\"\n");
  ClIncludeInliner inliner(platform, headers);
  CHECK(inliner.read_options("-Id") == av::Status::ok);
  const char source[] = "#include \"once.h\"\n#include \"once.h\"\n#include \"loop.h\"\n";
  CHECK(inliner.expand(source, sizeof source - 1u) == av::Status::ok);
  const char *at = std::strstr(inliner.text(), "int o;");
  CHECK(at != nullptr);
  CHECK(at != nullptr && std::strstr(at + 1, "int o;") == nullptr);
  CHECK(std::strstr(inliner.text(), "int l;") != nullptr); }

TEST(cl_program_source, the_text_and_options_of_a_program_are_kept_whole)
{ MockPlatform platform;
  ClProgramSource source(platform);
  const char *const parts[2] = {"int a;\n", "int b;\n"};
  CHECK(source.keep_text(parts, nullptr, 2u));
  CHECK_EQ(std::strcmp(source.text(), "int a;\nint b;\n"), 0);
  CHECK_EQ(source.text_bytes(), 14u);
  CHECK(source.options() == nullptr);
  CHECK(source.keep_options("-I x -D A"));
  CHECK(source.keep_host_options("-D A"));
  CHECK_EQ(std::strcmp(source.options(), "-I x -D A"), 0);
  CHECK_EQ(std::strcmp(source.host_options(), "-D A"), 0); }
