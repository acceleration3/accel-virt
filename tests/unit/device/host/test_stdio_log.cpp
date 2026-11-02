#include "device/host/stdio_log.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdio>
#include <cstring>

using av::StdioLog;

TEST(stdio_log, writes_each_line_whole_and_ended)
{ char       stored[64] = {};
  std::FILE *stream = fmemopen(stored, sizeof stored, "w");
  CHECK(stream != nullptr);
  StdioLog log(stream);
  log.write("one");
  log.write("two lines");
  std::fclose(stream);
  CHECK(std::strcmp(stored, "one\ntwo lines\n") == 0); }
