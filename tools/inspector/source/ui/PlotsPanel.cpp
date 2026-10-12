#include "ui/Inspector.h"

#include "text/Format.h"

namespace inspector {

void Inspector::PlotsPanel(ui::Context& ctx) {
    if (!ui::BeginWindow(ctx, "Plots", &m_show_plots)) {
        ui::EndWindow(ctx);
        return;
    }
    if (m_pinned.IsEmpty()) {
        ui::TextWrapped(ctx, "Nothing pinned. Click a number in State, or Plot calls/s under a thread's stack, "
                             "to follow it here.");
        ui::EndWindow(ctx);
        return;
    }
    usize remove = StringView::Npos;
    for (usize p = 0; p < m_pinned.GetSize(); ++p) {
        PinnedPlot& pinned = m_pinned[p];
        const Series* series = m_model.GetHistory().Find(pinned.series);
        DynamicArray<float> values;
        if (series != nullptr) {
            const usize count = series->values.GetSize();
            const usize first = count > 600 ? count - 600 : 0;
            for (usize i = first; i < count; ++i) {
                if (!pinned.rate) {
                    values.PushBack(static_cast<float>(series->values[i]));
                } else if (i > first) {
                    const f64 dt = series->times[i] - series->times[i - 1];
                    values.PushBack(dt > 0.0 ? static_cast<float>((series->values[i] - series->values[i - 1]) / dt) : 0.f);
                }
            }
        }
        ui::BeginRow(ctx, {ui::Size::Flex(1), ui::Size::Auto(), ui::Size::Auto()});
        String& title = ctx.GetScratchString();
        FormatTo(title, "{}{}", pinned.label, pinned.rate ? " (per second)" : "");
        ui::Text(ctx, title);
        if (ui::SmallButton(ctx, Format("{}##rate{}", pinned.rate ? "value" : "rate", p))) {
            pinned.rate = !pinned.rate;
        }
        if (ui::SmallButton(ctx, Format("remove##{}", p))) {
            remove = p;
        }
        ui::EndRow(ctx);
        String& overlay = ctx.GetScratchString();
        if (!values.IsEmpty()) {
            FormatTo(overlay, "now {:.1f}", values[values.GetSize() - 1]);
        }
        ui::PlotOptions options;
        options.overlay = overlay;
        options.height = 80.f;
        ui::PlotLines(ctx, Format("##plot{}", p), values.AsSpan(), options);
    }
    if (remove != StringView::Npos) {
        m_pinned.RemoveAt(remove);
    }
    ui::EndWindow(ctx);
}

} // namespace inspector
