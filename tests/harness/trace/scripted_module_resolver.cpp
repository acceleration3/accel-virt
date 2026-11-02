#include "harness/trace/scripted_module_resolver.hpp"

#include <cstring>

namespace av {

ScriptedModuleResolver::ScriptedModuleResolver() : modules_{}, count_(0u), asked_(0u) {}

bool ScriptedModuleResolver::resolve(std::uint64_t address, char *name, std::size_t capacity, std::uint64_t &offset)
{
    asked_++;
    for (std::uint32_t index = 0u; index < count_; index++) {
        const Module &module = modules_[index];
        if (address < module.base || address - module.base >= module.size) continue;
        const std::size_t length = std::strlen(module.name);
        std::memcpy(name, module.name, length < capacity ? length + 1u : capacity);
        offset = address - module.base;
        return true;
    }
    return false;
}

void ScriptedModuleResolver::add(std::uint64_t base, std::uint64_t size, const char *name)
{
    modules_[count_++] = Module{base, size, name};
}

std::uint32_t ScriptedModuleResolver::asked() const { return asked_; }

}
