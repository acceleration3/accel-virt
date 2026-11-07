#ifndef AV_DESCRIPTOR_TABLES_HPP
#define AV_DESCRIPTOR_TABLES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

template <typename Object, typename Pin>
class DescriptorTables {
public:
    struct Slot {
        Object *first_;
        Object *second_;
        Pin    *first_pin_;
        Pin    *second_pin_;
    };

    explicit DescriptorTables(BlockSource &blocks) : pool_(blocks), tables_(blocks, Pool::paged), blocks_(blocks), total_(0u)
    {
        AV_SCOPE("DescriptorTables::DescriptorTables");
    }
    DescriptorTables(const DescriptorTables &) = delete;
    DescriptorTables &operator=(const DescriptorTables &) = delete;

    [[nodiscard]] Status put(uint64_t owner, uint64_t key, const Slot &named, Slot &displaced)
    {
        AV_SCOPE("DescriptorTables::put");
        displaced = Slot{};
        const bool empty = named.first_ == nullptr && named.second_ == nullptr;
        Table     *table = nullptr;
        if (!table_of(owner, table)) {
            if (empty) return Status::ok;
            table = table_for(owner);
            if (table == nullptr) return Status::no_memory;
        }
        Slot *const found = table->slots_.find(key);
        if (empty) {
            if (found == nullptr) return Status::ok;
            displaced = *found;
            static_cast<void>(table->slots_.erase(key));
            total_--;
            drop_if_bare(owner, *table);
            return Status::ok;
        }
        if (found != nullptr) {
            displaced = *found;
            *found = named;
        } else {
            uint32_t     slot = 0u;
            const Status inserted = table->slots_.insert(key, named, slot);
            if (inserted != Status::ok) {
                drop_if_bare(owner, *table);
                return inserted;
            }
            total_++;
        }
        if (named.first_ != nullptr) named.first_->AddRef();
        if (named.second_ != nullptr) named.second_->AddRef();
        return Status::ok;
    }

    void submitted(uint64_t owner, uint64_t mark)
    {
        AV_SCOPE("DescriptorTables::submitted");
        Table *table = nullptr;
        if (table_of(owner, table) && mark > table->mark_) table->mark_ = mark;
    }

    template <typename Letter>
    [[nodiscard]] Status release(uint64_t owner, Letter let_go)
    {
        AV_SCOPE("DescriptorTables::release");
        Table *table = nullptr;
        if (!table_of(owner, table)) return Status::ok;
        for (uint32_t slot = table->slots_.next_live(0u); slot != KeyedSlots<uint64_t, Slot>::no_slot;
             slot = table->slots_.next_live(slot + 1u)) {
            Slot         named = *table->slots_.at(slot);
            const Status let = let_go(named, table->mark_);
            if (let != Status::ok) {
                *table->slots_.at(slot) = named;
                return let;
            }
            table->slots_.erase_slot(slot);
            total_--;
        }
        static_cast<void>(tables_.erase(owner));
        static_cast<void>(pool_.destroy(table));
        return Status::ok;
    }

    template <typename Visitor>
    void drain_all(Visitor visit)
    {
        AV_SCOPE("DescriptorTables::drain_all");
        for (uint32_t at = tables_.next_live(0u); at != KeyedSlots<uint64_t, Table *>::no_slot;
             at = tables_.next_live(0u)) {
            const uint64_t owner = tables_.key_at(at);
            uint32_t       cursor = 0u;
            for (;;) {
                Table *const *const found = tables_.find(owner);
                if (found == nullptr) break;
                Table *const   table = *found;
                const uint32_t slot = table->slots_.next_live(cursor);
                if (slot == KeyedSlots<uint64_t, Slot>::no_slot) {
                    static_cast<void>(tables_.erase(owner));
                    static_cast<void>(pool_.destroy(table));
                    break;
                }
                Slot named = *table->slots_.at(slot);
                table->slots_.erase_slot(slot);
                total_--;
                cursor = slot + 1u;
                visit(named);
            }
        }
    }

    template <typename Visitor>
    void for_each_slot(uint64_t owner, Visitor visit) const
    {
        AV_SCOPE("DescriptorTables::for_each_slot");
        Table *const *const found = tables_.find(owner);
        if (found == nullptr) return;
        (*found)->slots_.for_each([&visit](uint64_t, const Slot &named) { visit(named); });
    }

    const Slot *find(uint64_t owner, uint64_t key) const
    {
        AV_SCOPE("DescriptorTables::find");
        Table *const *const found = tables_.find(owner);
        return found != nullptr ? (*found)->slots_.find(key) : nullptr;
    }

    uint64_t mark_of(uint64_t owner) const
    {
        AV_SCOPE("DescriptorTables::mark_of");
        Table *const *const found = tables_.find(owner);
        return found != nullptr ? (*found)->mark_ : 0u;
    }

    bool known(uint64_t owner) const
    {
        AV_SCOPE("DescriptorTables::known");
        return tables_.find(owner) != nullptr;
    }

    uint32_t owners() const
    {
        AV_SCOPE("DescriptorTables::owners");
        return tables_.count();
    }

    uint32_t count() const
    {
        AV_SCOPE("DescriptorTables::count");
        return total_;
    }

    bool invariants_hold() const
    {
        AV_SCOPE("DescriptorTables::invariants_hold");
        if (!pool_.invariants_hold() || !tables_.invariants_hold() || pool_.live() != tables_.count()) return false;
        uint32_t counted = 0u;
        bool     sound = true;
        tables_.for_each([&](uint64_t, Table *const &table) {
            sound = sound && table != nullptr && table->slots_.invariants_hold() && table->slots_.count() != 0u;
            if (table == nullptr) return;
            counted += table->slots_.count();
            table->slots_.for_each([&sound](uint64_t, const Slot &named) {
                sound = sound && (named.first_ != nullptr || named.second_ != nullptr);
            });
        });
        return sound && counted == total_;
    }

private:
    struct Table {
        explicit Table(BlockSource &blocks) : slots_(blocks, Pool::paged), mark_(0u)
        {
            AV_SCOPE("DescriptorTables::Table::Table");
        }

        KeyedSlots<uint64_t, Slot> slots_;
        uint64_t                   mark_;
    };

    bool table_of(uint64_t owner, Table *&table) const
    {
        AV_SCOPE("DescriptorTables::table_of");
        Table *const *const found = tables_.find(owner);
        table = found != nullptr ? *found : nullptr;
        return found != nullptr;
    }

    Table *table_for(uint64_t owner)
    {
        AV_SCOPE("DescriptorTables::table_for");
        Table *made = nullptr;
        if (pool_.create(made, blocks_) != Status::ok) return nullptr;
        uint32_t slot = 0u;
        if (tables_.insert(owner, made, slot) == Status::ok) return made;
        static_cast<void>(pool_.destroy(made));
        return nullptr;
    }

    void drop_if_bare(uint64_t owner, Table &table)
    {
        AV_SCOPE("DescriptorTables::drop_if_bare");
        if (table.slots_.count() != 0u) return;
        static_cast<void>(tables_.erase(owner));
        static_cast<void>(pool_.destroy(&table));
    }

    ObjectPool<Table>            pool_;
    KeyedSlots<uint64_t, Table *> tables_;
    BlockSource                 &blocks_;
    uint32_t                     total_;
};

}

#endif
