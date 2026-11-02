#include "harness/framework/test_registry.hpp"

int main(int argc, char **argv) { return av::TestRegistry::instance().run(argc, argv); }
