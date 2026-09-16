/* Modules at addresses a test declares, for a crash report's native stack. */
#ifndef AV_SCRIPTED_MODULE_RESOLVER_HPP
#define AV_SCRIPTED_MODULE_RESOLVER_HPP

#include "guest/trace/module_resolver.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class ScriptedModuleResolver final : public ModuleResolver {
public:
    static constexpr std::uint32_t max_modules = 8u;

    ScriptedModuleResolver();

    bool resolve(std::uint64_t address, char *name, std::size_t capacity, std::uint64_t &offset) override;

    void          add(std::uint64_t base, std::uint64_t size, const char *name);
    std::uint32_t asked() const;

private:
    struct Module {
        std::uint64_t base;
        std::uint64_t size;
        const char   *name;
    };

    Module        modules_[max_modules];
    std::uint32_t count_;
    std::uint32_t asked_;
};

}  // namespace av

#endif  // AV_SCRIPTED_MODULE_RESOLVER_HPP
