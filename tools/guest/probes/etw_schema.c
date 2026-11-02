#define INITGUID
#include <windows.h>
#include <evntcons.h>
#include <tdh.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct {
    ULONG NumberOfEvents;
    ULONG Reserved;
    EVENT_DESCRIPTOR EventDescriptorsArray[ANYSIZE_ARRAY];
} ProviderEvents;
typedef ULONG(WINAPI *EnumerateManifestProviderEvents)(LPGUID provider, ProviderEvents *buffer, ULONG *size);
typedef ULONG(WINAPI *GetManifestEventInformation)(LPGUID provider, PEVENT_DESCRIPTOR descriptor,
                                                   PTRACE_EVENT_INFO buffer, ULONG *size);
enum { fixed_count = 0x20 };

static EnumerateManifestProviderEvents enumerate_events;
static GetManifestEventInformation event_information;
static FILE *out;

static void json_string(const wchar_t *text)
{
    fputc('"', out);
    for (; text && *text; text++) {
        wchar_t c = *text;
        if (c == L'"' || c == L'\\') fprintf(out, "\\%c", (char)c);
        else if (c < 0x20) fprintf(out, "\\u%04x", (unsigned)c);
        else if (c < 0x80) fputc((char)c, out);
        else {
            char utf8[8];
            int n = WideCharToMultiByte(CP_UTF8, 0, &c, 1, utf8, sizeof utf8, NULL, NULL);
            fwrite(utf8, 1, n > 0 ? (size_t)n : 0, out);
        }
    }
    fputc('"', out);
}

static const wchar_t *at(const void *base, ULONG offset)
{
    return offset ? (const wchar_t *)((const BYTE *)base + offset) : NULL;
}

static void *fetch_provider_events(const GUID *provider)
{
    ULONG size = 0;
    if (enumerate_events((LPGUID)provider, NULL, &size) != ERROR_INSUFFICIENT_BUFFER) return NULL;
    void *events = malloc(size);
    if (enumerate_events((LPGUID)provider, events, &size) != ERROR_SUCCESS) {
        free(events);
        return NULL;
    }
    return events;
}

static TRACE_EVENT_INFO *fetch_event(const GUID *provider, EVENT_DESCRIPTOR *descriptor)
{
    ULONG size = 0;
    if (event_information((LPGUID)provider, descriptor, NULL, &size) != ERROR_INSUFFICIENT_BUFFER)
        return NULL;
    TRACE_EVENT_INFO *info = malloc(size);
    if (event_information((LPGUID)provider, descriptor, info, &size) != ERROR_SUCCESS) {
        free(info);
        return NULL;
    }
    return info;
}

static EVENT_MAP_INFO *fetch_map(const GUID *provider, EVENT_DESCRIPTOR *descriptor, const wchar_t *name)
{
    EVENT_RECORD record;
    memset(&record, 0, sizeof record);
    record.EventHeader.ProviderId = *provider;
    record.EventHeader.EventDescriptor = *descriptor;
    ULONG size = 0;
    if (TdhGetEventMapInformation(&record, (PWSTR)name, NULL, &size) != ERROR_INSUFFICIENT_BUFFER) return NULL;
    EVENT_MAP_INFO *map = malloc(size);
    if (TdhGetEventMapInformation(&record, (PWSTR)name, map, &size) != ERROR_SUCCESS) {
        free(map);
        return NULL;
    }
    return map;
}

typedef struct {
    wchar_t name[128];
    EVENT_DESCRIPTOR descriptor;
} MapName;

static int remember_map(MapName *maps, int count, int capacity, const wchar_t *name, EVENT_DESCRIPTOR *descriptor)
{
    for (int i = 0; i < count; i++)
        if (!wcscmp(maps[i].name, name)) return count;
    if (count == capacity) return count;
    wcsncpy(maps[count].name, name, 127);
    maps[count].name[127] = 0;
    maps[count].descriptor = *descriptor;
    return count + 1;
}

static void write_fields(const TRACE_EVENT_INFO *info)
{
    fprintf(out, "\"top_level\": %lu, \"fields\": [", info->TopLevelPropertyCount);
    for (ULONG i = 0; i < info->PropertyCount; i++) {
        const EVENT_PROPERTY_INFO *field = &info->EventPropertyInfoArray[i];
        fprintf(out, "%s{\"name\": ", i ? ", " : "");
        json_string(at(info, field->NameOffset));
        if (field->Flags & PropertyStruct) {
            fprintf(out, ", \"struct\": [%u, %u]", field->structType.StructStartIndex,
                    field->structType.NumOfStructMembers);
        } else {
            fprintf(out, ", \"in\": %u, \"out\": %u", field->nonStructType.InType, field->nonStructType.OutType);
            if (field->nonStructType.MapNameOffset) {
                fprintf(out, ", \"map\": ");
                json_string(at(info, field->nonStructType.MapNameOffset));
            }
        }
        if (field->Flags & PropertyParamCount) fprintf(out, ", \"count_field\": %u", field->countPropertyIndex);
        else if (field->count > 1 || (field->Flags & fixed_count)) fprintf(out, ", \"count\": %u", field->count);
        if (field->Flags & PropertyParamLength) fprintf(out, ", \"length_field\": %u", field->lengthPropertyIndex);
        else if (field->length) fprintf(out, ", \"length\": %u", field->length);
        fprintf(out, "}");
    }
    fprintf(out, "]");
}

static void write_map(const EVENT_MAP_INFO *map, const wchar_t *name)
{
    json_string(name);
    fprintf(out, ": {\"bitmap\": %s, \"entries\": {", (map->Flag & EVENTMAP_INFO_FLAG_MANIFEST_BITMAP) ? "true" : "false");
    for (ULONG i = 0; i < map->EntryCount; i++) {
        fprintf(out, "%s\"%lu\": ", i ? ", " : "", map->MapEntryArray[i].Value);
        json_string(at(map, map->MapEntryArray[i].OutputOffset));
    }
    fprintf(out, "}}");
}

static int write_provider(const GUID *provider, const wchar_t *name, int first)
{
    ProviderEvents *events = fetch_provider_events(provider);
    if (!events) {
        fwprintf(stderr, L"no manifest events for %ls\n", name);
        return 0;
    }
    wchar_t guid[64];
    StringFromGUID2(provider, guid, 64);
    fprintf(out, "%s\n{\"name\": ", first ? "" : ",");
    json_string(name);
    fprintf(out, ", \"guid\": ");
    json_string(guid);
    fprintf(out, ", \"events\": [");
    static MapName maps[1024];
    int map_count = 0;
    int written = 0;
    for (ULONG i = 0; i < events->NumberOfEvents; i++) {
        EVENT_DESCRIPTOR *descriptor = &events->EventDescriptorsArray[i];
        TRACE_EVENT_INFO *info = fetch_event(provider, descriptor);
        if (!info) continue;
        fprintf(out, "%s\n  {\"id\": %u, \"version\": %u, \"opcode\": %u, \"task\": %u, \"level\": %u, "
                     "\"keywords\": \"0x%llx\", \"task_name\": ",
                written++ ? "," : "", descriptor->Id, descriptor->Version, descriptor->Opcode, descriptor->Task,
                descriptor->Level, (unsigned long long)descriptor->Keyword);
        json_string(at(info, info->TaskNameOffset));
        fprintf(out, ", \"opcode_name\": ");
        json_string(at(info, info->OpcodeNameOffset));
        fprintf(out, ", ");
        write_fields(info);
        fprintf(out, "}");
        for (ULONG p = 0; p < info->PropertyCount; p++) {
            const EVENT_PROPERTY_INFO *field = &info->EventPropertyInfoArray[p];
            if (!(field->Flags & PropertyStruct) && field->nonStructType.MapNameOffset)
                map_count = remember_map(maps, map_count, 1024, at(info, field->nonStructType.MapNameOffset), descriptor);
        }
        free(info);
    }
    fprintf(out, "],\n \"maps\": {");
    int maps_written = 0;
    for (int i = 0; i < map_count; i++) {
        EVENT_MAP_INFO *map = fetch_map(provider, &maps[i].descriptor, maps[i].name);
        if (!map) continue;
        fprintf(out, "%s\n  ", maps_written++ ? "," : "");
        write_map(map, maps[i].name);
        free(map);
    }
    fprintf(out, "}}");
    free(events);
    return 1;
}

static int find_provider(const wchar_t *wanted, GUID *guid)
{
    if (wanted[0] == L'{') return CLSIDFromString((LPCOLESTR)wanted, guid) == S_OK;
    ULONG size = 0;
    TdhEnumerateProviders(NULL, &size);
    PROVIDER_ENUMERATION_INFO *all = malloc(size);
    int found = 0;
    if (TdhEnumerateProviders(all, &size) == ERROR_SUCCESS) {
        for (ULONG i = 0; i < all->NumberOfProviders && !found; i++) {
            const TRACE_PROVIDER_INFO *provider = &all->TraceProviderInfoArray[i];
            if (!_wcsicmp(at(all, provider->ProviderNameOffset), wanted)) {
                *guid = provider->ProviderGuid;
                found = 1;
            }
        }
    }
    free(all);
    return found;
}

int wmain(int argc, wchar_t **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: etw_schema <out.json> <provider>...\n");
        return 2;
    }
    HMODULE tdh = LoadLibraryW(L"tdh.dll");
    enumerate_events = tdh ? (EnumerateManifestProviderEvents)(void *)GetProcAddress(tdh, "TdhEnumerateManifestProviderEvents") : NULL;
    event_information = tdh ? (GetManifestEventInformation)(void *)GetProcAddress(tdh, "TdhGetManifestEventInformation") : NULL;
    if (!enumerate_events || !event_information) {
        fprintf(stderr, "this Windows's tdh.dll cannot list a manifest's events\n");
        return 4;
    }
    out = _wfopen(argv[1], L"wb");
    if (!out) return 3;
    fprintf(out, "{\"providers\": [");
    int written = 0, missing = 0;
    for (int i = 2; i < argc; i++) {
        GUID guid;
        if (!find_provider(argv[i], &guid)) {
            fwprintf(stderr, L"no provider %ls\n", argv[i]);
            missing++;
            continue;
        }
        written += write_provider(&guid, argv[i], written == 0);
    }
    fprintf(out, "\n]}\n");
    fclose(out);
    return missing ? 1 : 0;
}
