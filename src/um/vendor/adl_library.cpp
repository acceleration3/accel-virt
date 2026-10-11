#include "um/vendor/adl_library.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

int AdlLibrary::create(bool has_callback, void **context)
{
    AV_SCOPE("AdlLibrary::create");
    vendor::AdlRoot *const answering = driver_.root();
    if (answering == nullptr) return vendor::AdlRoot::error;
    const int created = answering->create(has_callback);
    if (created == vendor::AdlRoot::ok && context != nullptr) *context = answering->context();
    return created;
}

int AdlLibrary::destroy(const void *context)
{
    AV_SCOPE("AdlLibrary::destroy");
    vendor::AdlRoot *const answering = root(context);
    return answering != nullptr ? answering->destroy() : vendor::AdlRoot::invalid_parameter;
}

int AdlLibrary::refresh(const void *context)
{
    AV_SCOPE("AdlLibrary::refresh");
    vendor::AdlRoot *const answering = root(context);
    return answering != nullptr ? answering->refresh() : vendor::AdlRoot::invalid_parameter;
}

int AdlLibrary::number_of_adapters(const void *context, int *count)
{
    AV_SCOPE("AdlLibrary::number_of_adapters");
    vendor::AdlRoot *const answering = root(context);
    return answering != nullptr ? answering->number_of_adapters(count) : vendor::AdlRoot::invalid_parameter;
}

int AdlLibrary::adapter_info(const void *context, vendor::AdlAdapterInfoRecord *infos, int bytes)
{
    AV_SCOPE("AdlLibrary::adapter_info");
    vendor::AdlRoot *const answering = root(context);
    return answering != nullptr ? answering->adapter_info(infos, bytes) : vendor::AdlRoot::invalid_parameter;
}

int AdlLibrary::active(const void *context, int index, int *status)
{
    AV_SCOPE("AdlLibrary::active");
    vendor::AdlRoot *const answering = root(context);
    return answering != nullptr ? answering->active(index, status) : vendor::AdlRoot::invalid_parameter;
}

int AdlLibrary::asic_family_type(const void *context, int index, int *types, int *valids)
{
    AV_SCOPE("AdlLibrary::asic_family_type");
    vendor::AdlRoot *const answering = root(context);
    return answering != nullptr ? answering->asic_family_type(index, types, valids)
                                : vendor::AdlRoot::invalid_parameter;
}

int AdlLibrary::primary(const void *context, int *index)
{
    AV_SCOPE("AdlLibrary::primary");
    vendor::AdlRoot *const answering = root(context);
    return answering != nullptr ? answering->primary(index) : vendor::AdlRoot::invalid_parameter;
}

int AdlLibrary::versions(const void *context, vendor::AdlVersionsInfoRecord *versions)
{
    AV_SCOPE("AdlLibrary::versions");
    vendor::AdlRoot *const answering = root(context);
    return answering != nullptr ? answering->versions(versions) : vendor::AdlRoot::invalid_parameter;
}

int AdlLibrary::versions_x2(const void *context, vendor::AdlVersionsInfoX2Record *versions)
{
    AV_SCOPE("AdlLibrary::versions_x2");
    vendor::AdlRoot *const answering = root(context);
    return answering != nullptr ? answering->versions_x2(versions) : vendor::AdlRoot::invalid_parameter;
}

void *AdlLibrary::proc_address(const void *context, const char *name)
{
    AV_SCOPE("AdlLibrary::proc_address");
    HMODULE self = nullptr;
    if (root(context) == nullptr ||
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(this), &self) == FALSE)
        return nullptr;
    return reinterpret_cast<void *>(GetProcAddress(self, name));
}

vendor::AdlRoot *AdlLibrary::root(const void *context)
{
    AV_SCOPE("AdlLibrary::root");
    vendor::AdlRoot *const answering = driver_.root();
    return answering != nullptr && answering->holds(context) ? answering : nullptr;
}

}  // namespace av
