// Unit tests only: a fuzz target links the harness too, and libFuzzer brings its own main.
#include "test_registry.hpp"

int main(int argc, char **argv) { return av::TestRegistry::instance().run(argc, argv); }
