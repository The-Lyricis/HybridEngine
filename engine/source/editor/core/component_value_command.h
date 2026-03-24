#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <imgui.h>

#include "editor/core/editor_command_history.h"
#include "editor/core/editor_context.h"
#include "editor/core/scene_document.h"
#include "runtime/modules/scene/entity.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    namespace Detail
    {
        template<typename TComponent>
        struct ContinuousEditState
        {
            TComponent before{};
            bool tracking = false;
        };

        template<typename TComponent>
        bool ApplyComponentSnapshot(Scene& scene, entt::entity entity_handle, const TComponent& snapshot)
        {
            auto& registry = scene.getRegistry();
            if (entity_handle == entt::null || !registry.valid(entity_handle) || !registry.all_of<TComponent>(entity_handle))
                return false;

            registry.get<TComponent>(entity_handle) = snapshot;
            return true;
        }

        template<>
        inline bool ApplyComponentSnapshot<TransformComponent>(Scene& scene, entt::entity entity_handle, const TransformComponent& snapshot)
        {
            auto& registry = scene.getRegistry();
            if (entity_handle == entt::null || !registry.valid(entity_handle) || !registry.all_of<TransformComponent>(entity_handle))
                return false;

            auto& transform = registry.get<TransformComponent>(entity_handle);
            transform.Position = snapshot.Position;
            transform.Rotation = snapshot.Rotation;
            transform.Scale = snapshot.Scale;
            transform.DirtyLocal = true;
            scene.MarkDirtyRecursive(Entity(entity_handle, &registry, &scene));
            return true;
        }
    } // namespace Detail

    template<typename TComponent>
    class SetComponentValueCommand final : public IEditorCommand
    {
    public:
        SetComponentValueCommand(std::shared_ptr<SceneDocument> document,
                                 entt::entity entity_handle,
                                 std::string name,
                                 TComponent before,
                                 TComponent after)
            : m_document(std::move(document))
            , m_entity_handle(entity_handle)
            , m_name(std::move(name))
            , m_before(std::move(before))
            , m_after(std::move(after))
        {
        }

        void undo(EditorContext& ctx) override
        {
            (void)apply(ctx, m_before);
        }

        void redo(EditorContext& ctx) override
        {
            (void)apply(ctx, m_after);
        }

        const char* name() const override
        {
            return m_name.c_str();
        }

    private:
        bool apply(EditorContext& ctx, const TComponent& snapshot) const
        {
            const std::shared_ptr<SceneDocument> document = m_document.lock();
            if (!document || !document->scene)
                return false;

            if (!Detail::ApplyComponentSnapshot<TComponent>(*document->scene, m_entity_handle, snapshot))
                return false;

            document->dirty = true;
            if (ctx.active_document == document)
                ctx.markSceneDirty();
            return true;
        }

    private:
        std::weak_ptr<SceneDocument> m_document;
        entt::entity m_entity_handle{entt::null};
        std::string m_name;
        TComponent m_before{};
        TComponent m_after{};
    };

    template<typename TComponent>
    void CommitComponentValueChange(EditorContext& ctx,
                                    Entity entity,
                                    const char* command_name,
                                    const TComponent& before,
                                    const TComponent& after)
    {
        if (!entity || !ctx.active_document || !ctx.submit_editor_command)
            return;
        if (ctx.is_play_mode && ctx.is_play_mode())
            return;

        auto command = std::make_unique<SetComponentValueCommand<TComponent>>(ctx.active_document,
                                                                              entity.GetHandle(),
                                                                              command_name,
                                                                              before,
                                                                              after);
        ctx.submit_editor_command(std::move(command));
    }

    template<typename TComponent, typename TDrawFn, typename TNormalizeFn>
    bool DrawTrackedContinuousComponentEdit(EditorContext& ctx,
                                            Entity entity,
                                            TComponent& component,
                                            const char* command_name,
                                            TDrawFn&& draw_fn,
                                            TNormalizeFn&& normalize_fn)
    {
        static std::unordered_map<ImGuiID, Detail::ContinuousEditState<TComponent>> s_edit_states;

        const TComponent before_for_frame = component;
        const bool changed = static_cast<bool>(draw_fn());
        if (changed)
            normalize_fn(component);

        const uint32_t entity_id = static_cast<uint32_t>(entt::to_integral(entity.GetHandle()));
        const size_t command_hash = std::hash<std::string_view>{}(std::string_view(command_name));
        const ImGuiID session_id = static_cast<ImGuiID>(command_hash ^ (static_cast<size_t>(entity_id) * 16777619ull));
        auto& state = s_edit_states[session_id];

        if (!state.tracking && (ImGui::IsItemActivated() || changed))
        {
            state.before = before_for_frame;
            state.tracking = true;
        }

        const bool ended =
            state.tracking &&
            (ImGui::IsItemDeactivatedAfterEdit() ||
             (!ImGui::IsItemActive() && !ImGui::IsMouseDown(ImGuiMouseButton_Left)));

        if (ended)
        {
            CommitComponentValueChange(ctx, entity, command_name, state.before, component);
            s_edit_states.erase(session_id);
        }

        return changed;
    }

    template<typename TComponent, typename TDrawFn>
    bool DrawTrackedContinuousComponentEdit(EditorContext& ctx,
                                            Entity entity,
                                            TComponent& component,
                                            const char* command_name,
                                            TDrawFn&& draw_fn)
    {
        return DrawTrackedContinuousComponentEdit(ctx,
                                                  entity,
                                                  component,
                                                  command_name,
                                                  std::forward<TDrawFn>(draw_fn),
                                                  [](TComponent&) {});
    }

    template<typename TComponent, typename TDrawFn, typename TNormalizeFn>
    bool DrawTrackedImmediateComponentEdit(EditorContext& ctx,
                                           Entity entity,
                                           TComponent& component,
                                           const char* command_name,
                                           TDrawFn&& draw_fn,
                                           TNormalizeFn&& normalize_fn)
    {
        const TComponent before = component;
        const bool changed = static_cast<bool>(draw_fn());
        if (!changed)
            return false;

        normalize_fn(component);
        CommitComponentValueChange(ctx, entity, command_name, before, component);
        return true;
    }

    template<typename TComponent, typename TDrawFn>
    bool DrawTrackedImmediateComponentEdit(EditorContext& ctx,
                                           Entity entity,
                                           TComponent& component,
                                           const char* command_name,
                                           TDrawFn&& draw_fn)
    {
        return DrawTrackedImmediateComponentEdit(ctx,
                                                 entity,
                                                 component,
                                                 command_name,
                                                 std::forward<TDrawFn>(draw_fn),
                                                 [](TComponent&) {});
    }
} // namespace Hybrid
