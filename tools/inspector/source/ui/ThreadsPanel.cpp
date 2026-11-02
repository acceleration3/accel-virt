#include "ui/Inspector.h"

#include "text/Format.h"

namespace inspector {
namespace {

graphics::Color StateColor(ThreadState state) {
    switch (state) {
    case ThreadState::Stalled: return palette::kBad;
    case ThreadState::Waiting: return palette::kWarn;
    case ThreadState::Running: return palette::kGood;
    case ThreadState::Idle: return palette::kMuted;
    }
    return palette::kMuted;
}

}

void Inspector::ThreadsPanel(ui::Context& ctx) {
    if (!ui::BeginWindow(ctx, "Threads", &m_show_threads)) {
        ui::EndWindow(ctx);
        return;
    }
    const ProcessSample* process = Selected();
    if (process == nullptr || !process->trace) {
        ui::TextDisabled(ctx, process == nullptr ? "Choose a process." : "This process left no trace table.");
        if (process != nullptr && !process->trace_error.IsEmpty()) {
            ui::TextColored(ctx, palette::kBad, process->trace_error);
        }
        ui::EndWindow(ctx);
        return;
    }
    const TraceTable& table = *process->trace;
    const String key = process->source.Key();
    const DynamicArray<ThreadView> all = m_model.Threads(*process, m_stall_seconds);

    usize counts[4] = {0, 0, 0, 0};
    for (const ThreadView& view : all) {
        ++counts[static_cast<usize>(view.state)];
    }
    ui::BeginRow(ctx);
    String& summary = ctx.GetScratchString();
    FormatTo(summary, "{} {}: ", table.module, table.pid);
    ui::Text(ctx, summary);
    for (usize s = 0; s < 4; ++s) {
        String& part = ctx.GetScratchString();
        FormatTo(part, "{} {}", counts[s], ThreadStateName(static_cast<ThreadState>(s)));
        ui::TextColored(ctx, counts[s] != 0 ? StateColor(static_cast<ThreadState>(s)) : palette::kMuted, part);
    }
    ui::EndRow(ctx);
    ui::BeginRow(ctx, {ui::Size::Fixed(130), ui::Size::Fixed(320)});
    ui::Checkbox(ctx, "Only active", m_only_active);
    ui::SliderFloat(ctx, "Stall after", m_stall_seconds, 0.5f, 30.f, "{:.1f} s");
    ui::EndRow(ctx);
    if (table.slots.IsEmpty() && process->source.alive) {
        ui::TextColored(ctx, palette::kWarn,
                        "No thread has ever entered a traced function: this binary's scopes are not bound.");
        if (table.module == StringView("device") || table.module == StringView("executor")) {
            ui::TextWrapped(ctx, "QEMU's device and executor record only when QEMU runs with AV_TRACE_SCOPES=1 "
                                 "(or AV_PERFETTO=1) in its environment. Restart the VM with it set to see their "
                                 "threads here.");
        }
    }
    if (table.HasCrashed()) {
        String& crash = ctx.GetScratchString();
        FormatTo(crash, "Crashed: code {:#x} at {:#x} on thread {}", table.crash_code, table.crash_address,
                 table.crash_thread);
        ui::TextColored(ctx, palette::kBad, crash);
    }

    DynamicArray<const ThreadView*> rows;
    for (const ThreadView& view : all) {
        if (!m_only_active || view.state != ThreadState::Idle) {
            rows.PushBack(&view);
        }
    }
    const ui::TableColumn columns[] = {
        {"Thread", ui::Size::Fixed(70), ui::Align::End},
        {"State", ui::Size::Fixed(70)},
        {"Function", ui::Size::Flex(3)},
        {"In it", ui::Size::Fixed(80), ui::Align::End},
        {"Depth", ui::Size::Fixed(50), ui::Align::End},
        {"Calls/s", ui::Size::Fixed(80), ui::Align::End},
        {"Lock", ui::Size::Flex(1.5f)},
    };
    ui::TableOptions options;
    options.size = Vector2F{0.f, ui::GetWindowContentRect(ctx).Height() * 0.55f};
    if (ui::BeginTable(ctx, "threads", columns, options)) {
        const ui::TableRows visible = ui::TableClip(ctx, rows.GetSize());
        for (usize i = visible.first; i < visible.last; ++i) {
            const ThreadView& view = *rows[i];
            const TraceSlot& slot = *view.slot;
            if (ui::TableNextRow(ctx, view.thread == m_selected_thread)) {
                m_selected_thread = view.thread;
            }
            String& thread = ctx.GetScratchString();
            FormatTo(thread, "{}", view.thread);
            ui::TableCellText(ctx, thread);
            ui::TableCellText(ctx, ThreadStateName(view.state), StateColor(view.state));
            if (!view.stack.IsEmpty()) {
                const StackFrameView& top = view.stack[view.stack.GetSize() - 1];
                ui::TableCellText(ctx, top.name);
                ui::TableCellText(ctx, FormatSeconds(top.age));
            } else {
                String& last = ctx.GetScratchString();
                FormatTo(last, "last: {}", view.last_left.IsEmpty() ? StringView("-") : view.last_left.GetView());
                ui::TableCellText(ctx, last, palette::kMuted);
                ui::TableCellText(ctx, view.left_ago > 0.0 ? FormatSeconds(view.left_ago) : String("-"), palette::kMuted);
            }
            String& depth = ctx.GetScratchString();
            FormatTo(depth, "{}", slot.depth);
            ui::TableCellText(ctx, depth);
            ui::TableCellText(ctx, view.calls_per_second ? FormatRate(*view.calls_per_second) : String("-"));
            String& lock = ctx.GetScratchString();
            if (slot.waiting_on != 0) {
                FormatTo(lock, "waits {:#x}", slot.waiting_on);
                if (view.lock_holder != nullptr) {
                    FormatTo(lock, " (held by {})", view.lock_holder->thread);
                }
                ui::TableCellText(ctx, lock, palette::kWarn);
            } else if (slot.holding != 0) {
                FormatTo(lock, "holds {:#x}", slot.holding);
                ui::TableCellText(ctx, lock);
            } else {
                ui::TableCellText(ctx, "");
            }
        }
        ui::EndTable(ctx);
    }

    const ThreadView* chosen = nullptr;
    for (const ThreadView& view : all) {
        if (view.thread == m_selected_thread) {
            chosen = &view;
        }
    }
    ui::SeparatorText(ctx, "Stack");
    if (chosen == nullptr) {
        ui::TextDisabled(ctx, "Choose a thread.");
    } else if (chosen->stack.IsEmpty()) {
        String& idle = ctx.GetScratchString();
        FormatTo(idle, "Thread {} is outside every tracked function; it last left {}{}.", chosen->thread,
                 chosen->last_left.IsEmpty() ? StringView("nothing") : chosen->last_left.GetView(),
                 chosen->left_ago > 0.0 ? Format(" {} ago", FormatSeconds(chosen->left_ago)) : String());
        ui::TextDisabled(ctx, idle);
    } else if (ui::BeginPanel(ctx, "stack")) {
        for (usize f = 0; f < chosen->stack.GetSize(); ++f) {
            const StackFrameView& frame = chosen->stack[f];
            String& line = ctx.GetScratchString();
            FormatTo(line, "{:>3}  {}{}", f, String(f * 2, ' '), frame.name);
            ui::BeginRow(ctx, {ui::Size::Flex(1), ui::Size::Fixed(90)});
            ui::Text(ctx, line);
            ui::TextColored(ctx, frame.age >= m_stall_seconds ? palette::kWarn : palette::kMuted, FormatSeconds(frame.age));
            ui::EndRow(ctx);
        }
        if (chosen->slot->depth > chosen->stack.GetSize()) {
            String& more = ctx.GetScratchString();
            FormatTo(more, "... {} deeper frames not stored", chosen->slot->depth - chosen->stack.GetSize());
            ui::TextDisabled(ctx, more);
        }
        const String series = Format("{} thread/{} entries", key, chosen->thread);
        if (ui::SmallButton(ctx, IsPinned(series) ? "Unpin calls/s" : "Plot calls/s")) {
            TogglePin(series, Format("{} thread {} calls/s", key, chosen->thread), true);
        }
        ui::EndPanel(ctx);
    }
    ui::EndWindow(ctx);
}

}
