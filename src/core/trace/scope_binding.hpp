#ifndef AV_SCOPE_BINDING_HPP
#define AV_SCOPE_BINDING_HPP

namespace av {

class TraceTable;

class ScopeBinding {
public:
    constexpr ScopeBinding() = default;
    ScopeBinding(const ScopeBinding &) = delete;
    ScopeBinding &operator=(const ScopeBinding &) = delete;

    void        bind(TraceTable &table);
    void        unbind();
    TraceTable *table() const;

private:
    TraceTable *table_ = nullptr;
};

#if defined(__ELF__)
[[gnu::visibility("hidden")]]
#endif
extern constinit ScopeBinding scope_binding;

inline TraceTable *ScopeBinding::table() const { return table_; }

}

#endif
