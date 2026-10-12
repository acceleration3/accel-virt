#include "ui/Inspector.h"

#include "text/Format.h"

namespace inspector {

void Inspector::ProcessesPanel(ui::Context& ctx) {
    if (!ui::BeginWindow(ctx, "Processes", &m_show_processes)) {
        ui::EndWindow(ctx);
        return;
    }
    const Optional<Sample>& current = m_model.GetCurrent();
    if (!current) {
        ui::TextDisabled(ctx, "Waiting for the first round.");
        ui::EndWindow(ctx);
        return;
    }
    if (current->processes.IsEmpty()) {
        ui::TextWrapped(ctx, "No accel-virt binary on this host: no av-trace-* or av-state-* in the shared "
                             "memory directory. Start a VM with virtio-gpu-av-pci.");
        ui::EndWindow(ctx);
        return;
    }
    const ui::TableColumn columns[] = {
        {"Process", ui::Size::Flex(2)},
        {"Threads", ui::Size::Fixed(70), ui::Align::End},
        {"Dump", ui::Size::Flex(1)},
    };
    ui::TableOptions options;
    options.default_sort_column = NullOpt;
    if (ui::BeginTable(ctx, "processes", columns, options)) {
        for (const ProcessSample& process : current->processes) {
            const String key = process.source.Key();
            if (ui::TableNextRow(ctx, key == m_selected)) {
                m_selected = key;
                m_selected_thread = 0;
            }
            const bool crashed = process.trace && process.trace->HasCrashed();
            const graphics::Color name_color = crashed                ? palette::kBad
                                               : !process.source.alive ? palette::kMuted
                                                                       : ctx.GetColor(ui::ColorRole::Text);
            String& name = ctx.GetScratchString();
            FormatTo(name, "{} {}{}{}", process.source.module, process.source.pid, process.source.alive ? "" : " (gone)",
                     crashed ? " CRASHED" : "");
            ui::TableCellText(ctx, name, name_color);
            String& threads = ctx.GetScratchString();
            if (process.trace) {
                usize active = 0;
                for (const TraceSlot& slot : process.trace->slots) {
                    active += slot.active ? 1 : 0;
                }
                FormatTo(threads, "{}/{}", active, process.trace->slots.GetSize());
            } else {
                FormatTo(threads, "-");
            }
            ui::TableCellText(ctx, threads);
            String& dump = ctx.GetScratchString();
            if (process.state) {
                FormatTo(dump, "{} objects, {} ms", process.state->GetNodes().GetSize(),
                         static_cast<int>(process.state_seconds * 1000.0));
                ui::TableCellText(ctx, dump, process.state_seconds > 0.5 ? palette::kWarn : palette::kGood);
            } else if (!process.source.alive) {
                ui::TableCellText(ctx, "no process", palette::kMuted);
            } else if (process.source.state_path.IsEmpty()) {
                ui::TableCellText(ctx, "serves none", palette::kMuted);
            } else {
                ui::TableCellText(ctx, process.state_error, palette::kBad);
            }
            if (ui::IsTableRowHovered(ctx) && (crashed || !process.trace_error.IsEmpty())) {
                ui::BeginTooltip(ctx);
                if (crashed) {
                    String& tip = ctx.GetScratchString();
                    FormatTo(tip, "crash code {:#x} at {:#x} on thread {}", process.trace->crash_code,
                             process.trace->crash_address, process.trace->crash_thread);
                    ui::Text(ctx, tip);
                }
                if (!process.trace_error.IsEmpty()) {
                    ui::Text(ctx, process.trace_error);
                }
                ui::EndTooltip(ctx);
            }
        }
        ui::EndTable(ctx);
    }
    ui::EndWindow(ctx);
}

} // namespace inspector
