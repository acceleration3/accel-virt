#include "core/support/vulkan_layer_filter.hpp"
#include "harness/framework/test_registry.hpp"

#include <string.h>

using av::LayerEnvironment;
using av::VulkanLayerFilter;

namespace {

class FakeEnvironment final : public LayerEnvironment {
public:
    FakeEnvironment() : count_(0) {}

    void set(const char *name, const char *value)
    {
        names_[count_] = name;
        values_[count_++] = value;
    }

    const char *value(const char *name) const override
    {
        for (int i = 0; i < count_; ++i)
            if (strcmp(names_[i], name) == 0) return values_[i];
        return nullptr;
    }

private:
    const char *names_[8];
    const char *values_[8];
    int         count_;
};

const char *lsfg_manifest =
    "{\n \"file_format_version\": \"1.0.0\",\n \"layer\": {\n  \"name\": \"VK_LAYER_LS_frame_generation\",\n"
    "  \"disable_environment\": {\n   \"DISABLE_LSFGVK\": \"1\"\n  }\n }\n}\n";

const char *steam_manifest =
    "{\"layers\": [{\"name\": \"A\", \"disable_environment\": {\"DISABLE_VK_LAYER_VALVE_steam_overlay_1\": \"1\"}},"
    " {\"name\": \"B\", \"disable_environment\": {\"DISABLE_B\": \"\", \"DISABLE_C\": \"yes\"}}]}";

}

TEST(vulkan_layer_filter, disables_the_implicit_group_and_each_layers_own_variable)
{ FakeEnvironment environment;
  VulkanLayerFilter filter(environment);
  filter.add_manifest(lsfg_manifest);
  filter.add_manifest(steam_manifest);
  filter.plan();
  CHECK(filter.count() == 5u);
  CHECK(strcmp(filter.name(0), "VK_LOADER_LAYERS_DISABLE") == 0);
  CHECK(strcmp(filter.value(0), "~implicit~") == 0);
  CHECK(strcmp(filter.name(1), "DISABLE_LSFGVK") == 0);
  CHECK(strcmp(filter.value(1), "1") == 0);
  CHECK(strcmp(filter.name(2), "DISABLE_VK_LAYER_VALVE_steam_overlay_1") == 0);
  CHECK(strcmp(filter.name(3), "DISABLE_B") == 0);
  CHECK(strcmp(filter.value(3), "1") == 0);
  CHECK(strcmp(filter.name(4), "DISABLE_C") == 0);
  CHECK(strcmp(filter.value(4), "yes") == 0); }

TEST(vulkan_layer_filter, without_manifests_it_still_sets_the_group)
{ FakeEnvironment environment;
  VulkanLayerFilter filter(environment);
  filter.plan();
  CHECK(filter.count() == 1u);
  CHECK(strcmp(filter.name(0), "VK_LOADER_LAYERS_DISABLE") == 0); }

TEST(vulkan_layer_filter, leaves_an_explicit_disable_setting_alone)
{ FakeEnvironment environment;
  environment.set("VK_LOADER_LAYERS_DISABLE", "VK_LAYER_x");
  VulkanLayerFilter filter(environment);
  filter.add_manifest(lsfg_manifest);
  filter.plan();
  CHECK(filter.count() == 0u); }

TEST(vulkan_layer_filter, leaves_an_explicit_enable_setting_alone)
{ FakeEnvironment environment;
  environment.set("VK_LOADER_LAYERS_ENABLE", "*validation*");
  VulkanLayerFilter filter(environment);
  filter.add_manifest(lsfg_manifest);
  filter.plan();
  CHECK(filter.count() == 0u); }

TEST(vulkan_layer_filter, an_empty_setting_is_not_an_explicit_choice)
{ FakeEnvironment environment;
  environment.set("VK_LOADER_LAYERS_DISABLE", "");
  VulkanLayerFilter filter(environment);
  filter.plan();
  CHECK(filter.count() == 1u); }

TEST(vulkan_layer_filter, skips_variables_the_user_set_and_repeats)
{ FakeEnvironment environment;
  environment.set("DISABLE_LSFGVK", "0");
  VulkanLayerFilter filter(environment);
  filter.add_manifest(lsfg_manifest);
  filter.add_manifest(steam_manifest);
  filter.add_manifest(steam_manifest);
  filter.add_manifest("{\"layer\": {\"disable_environment\": {\"DISABLE_C\": \"1\"}}}");
  filter.plan();
  CHECK(filter.count() == 4u);
  CHECK(strcmp(filter.name(1), "DISABLE_VK_LAYER_VALVE_steam_overlay_1") == 0); }

TEST(vulkan_layer_filter, ignores_manifests_without_a_disable_table)
{ FakeEnvironment environment;
  VulkanLayerFilter filter(environment);
  filter.add_manifest(nullptr);
  filter.add_manifest("{\"layer\": {\"name\": \"x\"}}");
  filter.add_manifest("{\"disable_environment\": \"oops\"}");
  filter.add_manifest("{\"disable_environment\": {\"DISABLE_X\"");
  filter.plan();
  CHECK(filter.count() == 1u);
  CHECK(strcmp(filter.name(5), "") == 0);
  CHECK(strcmp(filter.value(5), "") == 0); }
