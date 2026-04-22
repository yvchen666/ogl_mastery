#pragma once
#include <cstdint>
#include <unordered_map>
#include <typeindex>
#include <any>
#include <functional>
#include <vector>

using EntityId = uint32_t;
constexpr EntityId INVALID_ENTITY = 0;

// ─────────────────────────────────────────────────────────────────────────────
// 极简 ECS：实体只是 ID，Component 是 std::any
// ─────────────────────────────────────────────────────────────────────────────
class World {
public:
    EntityId create_entity() { return next_id_++; }

    void destroy_entity(EntityId e) {
        for (auto& [ti, map] : components_)
            map.erase(e);
        // 从实体列表中移除
        entities_.erase(std::find(entities_.begin(), entities_.end(), e));
    }

    template<typename T>
    void add_component(EntityId e, T comp) {
        // 首次使用某类型时，顺序保存实体 ID
        if (components_[typeid(T)].empty()) {}
        components_[typeid(T)][e] = std::move(comp);
        // 追踪实体（避免重复）
        if (std::find(entities_.begin(), entities_.end(), e) == entities_.end())
            entities_.push_back(e);
    }

    template<typename T>
    T* get_component(EntityId e) {
        auto it_type = components_.find(typeid(T));
        if (it_type == components_.end()) return nullptr;
        auto it = it_type->second.find(e);
        if (it == it_type->second.end()) return nullptr;
        return std::any_cast<T>(&it->second);
    }

    template<typename T>
    void each(std::function<void(EntityId, T&)> fn) {
        auto it = components_.find(typeid(T));
        if (it == components_.end()) return;
        for (auto& [eid, val] : it->second)
            fn(eid, std::any_cast<T&>(val));
    }

    const std::vector<EntityId>& all_entities() const { return entities_; }

private:
    EntityId next_id_{1};
    std::unordered_map<std::type_index,
        std::unordered_map<EntityId, std::any>> components_;
    std::vector<EntityId> entities_;
};
