#include "ui/Inspector.h"

#include "text/Format.h"
#include "text/NumberConversion.h"

namespace inspector {
namespace {

// ShareOrigin (src/executor/sharing/share.hpp).
StringView OriginName(StringView value) {
    if (value == "0") return "object";
    if (value == "1") return "D3D11";
    if (value == "2") return "D3D12";
    return value;
}

// SharedObjectKind (src/proto/resources/shared_object_kind.hpp); 0 is a Direct3D resource, which
// has no wire kind of its own.
StringView KindName(StringView value) {
    if (value == "0") return "resource";
    if (value == "1") return "memory";
    if (value == "2") return "monitored fence";
    if (value == "3") return "native fence";
    return value;
}

// What a size is likely to be: a frame of a common resolution at 4 or 8 bytes a pixel. A guess,
// shown as one: the dump says the bytes, not the shape.
String ShapeGuess(u64 size) {
    struct Mode {
        u32 width, height;
    };
    const Mode modes[] = {{1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}, {1920, 1200}, {2560, 1080},
                          {2560, 1440}, {2560, 1600}, {3440, 1440}, {3840, 2160}, {5120, 1440}, {7680, 4320}};
    for (const Mode& mode : modes) {
        const u64 pixels = static_cast<u64>(mode.width) * mode.height;
        if (size == pixels * 4) {
            return Format("{}x{} x4B?", mode.width, mode.height);
        }
        if (size == pixels * 8) {
            return Format("{}x{} x8B?", mode.width, mode.height);
        }
    }
    return {};
}

} // namespace

void Inspector::SurfacesPanel(ui::Context& ctx) {
    if (!ui::BeginWindow(ctx, "Surfaces", &m_show_surfaces)) {
        ui::EndWindow(ctx);
        return;
    }
    const Optional<Sample>& current = m_model.GetCurrent();
    if (!current) {
        ui::EndWindow(ctx);
        return;
    }
    bool any = false;
    for (const ProcessSample& process : current->processes) {
        if (!process.state) {
            continue;
        }
        const String process_key = process.source.Key();
        const StateDump& dump = *process.state;
        for (const StateNode* gpu : dump.OfKind("gpu_shares")) {
            any = true;
            const StateNode* table = nullptr;
            const StateNode* images = nullptr;
            for (const usize child : gpu->children) {
                const StateNode& node = dump.GetNodes()[child];
                if (node.kind == StringView("shares")) table = &node;
                if (node.kind == StringView("exportable_images")) images = &node;
            }
            u64 memory_bytes = 0;
            usize memory_count = 0;
            if (table != nullptr) {
                for (const usize child : table->children) {
                    const StateNode& share = dump.GetNodes()[child];
                    if (share.Value("kind") == StringView("1")) {
                        memory_bytes += static_cast<u64>(share.Integer("size").ValueOr(0));
                        ++memory_count;
                    }
                }
            }
            String& title = ctx.GetScratchString();
            FormatTo(title, "GPU {} ({}): {} shares, {} of memory in {}", gpu->index, process_key,
                     table != nullptr ? table->Value("live") : StringView("?"), FormatBytes(memory_bytes), memory_count);
            if (!ui::CollapsingHeader(ctx, title, true)) {
                continue;
            }
            if (table != nullptr && table->Flag("busy")) {
                ui::TextColored(ctx, palette::kWarn, "The share table was held by a worker when asked; next round.");
                continue;
            }
            String& summary = ctx.GetScratchString();
            if (table != nullptr) {
                FormatTo(summary, "objects {}, D3D11 {}, D3D12 {}, released by guest {}, unindexed {}, holders {}, retiring {}",
                         table->Value("objects"), table->Value("d3d11"), table->Value("d3d12"),
                         table->Value("released_by_guest"), table->Value("unindexed"), table->Value("holders"),
                         table->Value("retiring"));
            }
            if (images != nullptr) {
                FormatTo(summary, "  |  exportable images: {} live, {} retired, {} blocks", images->Value("live"),
                         images->Value("retired"), images->Value("blocks"));
            }
            ui::TextDisabled(ctx, summary);
            if (table == nullptr || table->children.IsEmpty()) {
                continue;
            }
            const ui::TableColumn columns[] = {
                {"Share", ui::Size::Flex(0.6f), ui::Align::End},
                {"Origin", ui::Size::Flex(0.7f)},
                {"Kind", ui::Size::Flex(1.1f)},
                {"Size", ui::Size::Flex(0.9f), ui::Align::End},
                {"Looks like", ui::Size::Flex(1.3f)},
                {"Guest", ui::Size::Flex(0.6f)},
                {"Pub", ui::Size::Flex(0.45f)},
                {"Exp", ui::Size::Flex(0.45f)},
                {"Hold", ui::Size::Flex(0.5f), ui::Align::End},
                {"Opens", ui::Size::Flex(0.6f), ui::Align::End},
                {"Writes", ui::Size::Flex(0.8f), ui::Align::End},
                {"Writes/s", ui::Size::Flex(0.9f), ui::Align::End},
            };
            ui::TableOptions options;
            options.size = Vector2F{0.f, 30.f + 24.f * static_cast<float>(table->children.GetSize() < 14 ? table->children.GetSize() : 14)};
            if (ui::BeginTable(ctx, Format("shares{}", gpu->path), columns, options)) {
                const ui::TableRows rows = ui::TableClip(ctx, table->children.GetSize());
                for (usize i = rows.first; i < rows.last; ++i) {
                    const StateNode& share = dump.GetNodes()[table->children[i]];
                    ui::TableNextRow(ctx);
                    const bool released = !share.Flag("guest_holds");
                    const graphics::Color text = released ? palette::kMuted : ctx.GetColor(ui::ColorRole::Text);
                    ui::TableCellText(ctx, share.index, text);
                    ui::TableCellText(ctx, OriginName(share.Value("origin")), text);
                    ui::TableCellText(ctx, KindName(share.Value("kind")), text);
                    const u64 size = static_cast<u64>(share.Integer("size").ValueOr(0));
                    ui::TableCellText(ctx, size != 0 ? FormatBytes(size) : String("-"), text);
                    ui::TableCellText(ctx, ShapeGuess(size), palette::kInfo);
                    ui::TableCellText(ctx, released ? "freed" : "holds", released ? palette::kWarn : palette::kGood);
                    ui::TableCellText(ctx, share.Flag("published") ? "yes" : "-", text);
                    ui::TableCellText(ctx, share.Flag("exported") ? "yes" : "-", text);
                    ui::TableCellText(ctx, share.Value("holders"), text);
                    ui::TableCellText(ctx, share.Value("opens"), text);
                    ui::TableCellText(ctx, share.Value("writes"), text);
                    const Optional<f64> writes = m_model.RateOf(process_key, share.path, "writes");
                    ui::TableCellText(ctx, writes && *writes > 0.0 ? FormatRate(*writes) : String("-"),
                                      writes && *writes > 0.0 ? palette::kGood : palette::kMuted);
                }
                ui::EndTable(ctx);
            }
        }
    }
    if (!any) {
        ui::TextDisabled(ctx, "No executor reports shares: nothing is shared between the guest's devices and the host.");
    }
    ui::EndWindow(ctx);
}

} // namespace inspector
