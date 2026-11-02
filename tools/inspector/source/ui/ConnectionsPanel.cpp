#include "ui/Inspector.h"

#include "core/Pair.h"
#include "text/Format.h"
#include "text/NumberConversion.h"

namespace inspector {
namespace {
DynamicArray<float> Derivative(const Series& series, usize most) {
    DynamicArray<float> rates;
    const usize count = series.values.GetSize();
    const usize first = count > most + 1 ? count - most - 1 : 0;
    for (usize i = first + 1; i < count; ++i) {
        const f64 dt = series.times[i] - series.times[i - 1];
        rates.PushBack(dt > 0.0 ? static_cast<float>((series.values[i] - series.values[i - 1]) / dt) : 0.f);
    }
    return rates;
}

}

void Inspector::ConnectionsPanel(ui::Context& ctx) {
    if (!ui::BeginWindow(ctx, "Connections", &m_show_connections)) {
        ui::EndWindow(ctx);
        return;
    }
    const Optional<Sample>& current = m_model.GetCurrent();
    if (!current) {
        ui::EndWindow(ctx);
        return;
    }
    const DynamicArray<ContextView> contexts = m_model.Contexts();
    DynamicArray<Pair<const ProcessSample*, const StateNode*>> sessions;
    for (const ProcessSample& process : current->processes) {
        if (!process.state) {
            continue;
        }
        for (const StateNode* node : process.state->OfKind("session")) {
            sessions.PushBack({&process, node});
        }
    }

    ui::SeparatorText(ctx, "Device contexts");
    if (contexts.IsEmpty()) {
        ui::TextDisabled(ctx, "No device has a context: no guest driver has opened one.");
    }
    const ui::TableColumn columns[] = {
        {"GPU", ui::Size::Fixed(40), ui::Align::End},
        {"Context", ui::Size::Fixed(70), ui::Align::End},
        {"API", ui::Size::Fixed(80)},
        {"Fence", ui::Size::Fixed(90), ui::Align::End},
        {"Batches/s", ui::Size::Fixed(90), ui::Align::End},
        {"Ring read/s", ui::Size::Fixed(100), ui::Align::End},
        {"Pending", ui::Size::Fixed(60), ui::Align::End},
        {"State", ui::Size::Flex(1)},
    };
    ui::TableOptions options;
    options.size = Vector2F{0.f, contexts.IsEmpty() ? 60.f : 30.f + 26.f * static_cast<float>(contexts.GetSize())};
    if (!contexts.IsEmpty() && ui::BeginTable(ctx, "contexts", columns, options)) {
        for (const ContextView& view : contexts) {
            const StateNode& node = *view.node;
            if (ui::TableNextRow(ctx, node.path == m_selected_context)) {
                m_selected_context = node.path;
            }
            ui::TableCellText(ctx, view.gpu);
            ui::TableCellText(ctx, node.index);
            String& api = ctx.GetScratchString();
            FormatTo(api, "{} {}", view.api, node.Value("version"));
            ui::TableCellText(ctx, api);
            ui::TableCellText(ctx, node.Value("last_fence"));
            const Optional<f64>& batches = view.batches_per_second;
            ui::TableCellText(ctx, batches ? FormatRate(*batches) : String("-"),
                              batches && *batches > 0.0 ? palette::kGood : palette::kMuted);
            ui::TableCellText(ctx, view.ring_bytes_per_second
                                       ? Format("{}/s", FormatBytes(static_cast<u64>(*view.ring_bytes_per_second)))
                                       : String("-"));
            ui::TableCellText(ctx, view.worker != nullptr ? view.worker->Value("pending") : StringView("-"));
            ui::TableCellText(ctx, view.state,
                              view.state == StringView("live")      ? palette::kGood
                              : view.state == StringView("FAULTED") ? palette::kBad
                                                                     : palette::kWarn);
        }
        ui::EndTable(ctx);
    }

    for (const ContextView& view : contexts) {
        if (view.node->path != m_selected_context) {
            continue;
        }
        const String process_key = view.process->source.Key();
        String& title = ctx.GetScratchString();
        FormatTo(title, "Context {} over time", view.node->index);
        ui::SeparatorText(ctx, title);
        const Pair<StringView, StringView> plots[] = {{"batches", "batches/s"}, {"head", "ring bytes/s"}};
        ui::BeginRow(ctx, {ui::Size::Flex(1), ui::Size::Flex(1)});
        for (const auto& plot : plots) {
            const Series* series = m_model.GetHistory().Find(History::SeriesKey(process_key, view.node->path, plot.first));
            if (series == nullptr) {
                ui::TextDisabled(ctx, "no samples yet");
                continue;
            }
            const DynamicArray<float> rates = Derivative(*series, 240);
            ui::PlotOptions options_plot;
            options_plot.overlay = plot.second;
            options_plot.height = 90.f;
            ui::PlotLines(ctx, Format("##{}{}", view.node->path, plot.first), rates.AsSpan(), options_plot);
        }
        ui::EndRow(ctx);
        String& fields = ctx.GetScratchString();
        FormatTo(fields, "ring order {} (ring {}), doorbell {}, executor {}, head {}", view.node->Value("ring_order"),
                 FormatBytes(1ull << ParseInt<u32>(view.node->Value("ring_order")).ValueOr(0)),
                 view.node->Value("doorbell"), view.node->Value("executor"), view.node->Value("head"));
        ui::TextDisabled(ctx, fields);
    }

    ui::SeparatorText(ctx, "Executor sessions");
    if (sessions.IsEmpty()) {
        ui::TextDisabled(ctx, "No session: the executor runs nothing for the guest.");
    } else {
        const ui::TableColumn session_columns[] = {
            {"Session", ui::Size::Fixed(70), ui::Align::End},
            {"API", ui::Size::Fixed(90)},
            {"GPU", ui::Size::Fixed(50), ui::Align::End},
            {"Domain", ui::Size::Fixed(70), ui::Align::End},
            {"Executor", ui::Size::Flex(1)},
        };
        ui::TableOptions session_options;
        session_options.size = Vector2F{0.f, 30.f + 26.f * static_cast<float>(sessions.GetSize())};
        if (ui::BeginTable(ctx, "sessions", session_columns, session_options)) {
            for (const auto& session : sessions) {
                ui::TableNextRow(ctx);
                ui::TableCellText(ctx, session.second->index);
                ui::TableCellText(ctx, session.second->Value("api"));
                ui::TableCellText(ctx, session.second->Value("gpu"));
                ui::TableCellText(ctx, session.second->Value("domain"));
                ui::TableCellText(ctx, session.first->source.Key(), palette::kMuted);
            }
            ui::EndTable(ctx);
        }
    }
    ui::EndWindow(ctx);
}

}
