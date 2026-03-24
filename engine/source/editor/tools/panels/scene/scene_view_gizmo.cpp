#include "scene_view_gizmo.h"

#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include "runtime/core/base/math_util.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/entity.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    namespace
    {
        glm::mat4 BuildModel(const TransformComponent& tr)
        {
            return tr.WorldMatrix;
        }
    } // namespace

    SceneViewGizmoResult HandleSceneViewGizmo(EditorContext& ctx,
                                              const ImVec2& viewport_min,
                                              const ImVec2& canvas_size,
                                              SceneViewGizmoDragState& drag_state)
    {
        SceneViewGizmoResult result{};
        ctx.gizmo.using_gizmo = false;

        const bool want_gizmo =
            (ctx.gizmo.tool_mode == SceneToolMode::Move ||
             ctx.gizmo.tool_mode == SceneToolMode::Rotate ||
             ctx.gizmo.tool_mode == SceneToolMode::Scale);

        if (want_gizmo && ctx.document.activeScene() && ctx.activeEntity() != entt::null)
        {
            auto& reg = ctx.document.activeScene()->getRegistry();
            if (reg.valid(ctx.activeEntity()) && reg.all_of<TransformComponent>(ctx.activeEntity()))
            {
                auto& tr = reg.get<TransformComponent>(ctx.activeEntity());
                const TransformSnapshot before_snapshot = CaptureTransformSnapshot(tr);
                glm::mat4 model = BuildModel(tr);

                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(viewport_min.x, viewport_min.y, canvas_size.x, canvas_size.y);

                ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
                if (ctx.gizmo.tool_mode == SceneToolMode::Rotate)
                    op = ImGuizmo::ROTATE;
                if (ctx.gizmo.tool_mode == SceneToolMode::Scale)
                    op = ImGuizmo::SCALE;

                ImGuizmo::MODE gizmo_mode = ImGuizmo::LOCAL;
                if (op != ImGuizmo::SCALE)
                    gizmo_mode = (ctx.gizmo.space == GizmoSpace::World) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

                bool snapping = ImGui::GetIO().KeyCtrl;
                float snap[3] = {0.5f, 0.5f, 0.5f};
                if (op == ImGuizmo::ROTATE)
                {
                    snap[0] = 15.0f;
                    snap[1] = 15.0f;
                    snap[2] = 15.0f;
                }
                if (op == ImGuizmo::SCALE)
                {
                    snap[0] = 0.1f;
                    snap[1] = 0.1f;
                    snap[2] = 0.1f;
                }

                ImGuizmo::Manipulate(
                    glm::value_ptr(ctx.gizmo.view),
                    glm::value_ptr(ctx.gizmo.proj),
                    op,
                    gizmo_mode,
                    glm::value_ptr(model),
                    nullptr,
                    snapping ? snap : nullptr);

                result.using_gizmo = ImGuizmo::IsUsing();
                result.gizmo_over = ImGuizmo::IsOver();
                ctx.gizmo.using_gizmo = result.using_gizmo;

                if (result.using_gizmo && !drag_state.drag_active)
                {
                    drag_state.drag_active = true;
                    drag_state.drag_entity = ctx.activeEntity();
                    drag_state.drag_before = before_snapshot;
                }

                if (result.using_gizmo)
                {
                    glm::vec3 skew{};
                    glm::vec4 perspective{};
                    glm::vec3 scale{};
                    glm::quat rotation{};
                    glm::vec3 translation{};
                    glm::mat4 local = model;
                    if (tr.Parent != entt::null && reg.valid(tr.Parent) && reg.all_of<TransformComponent>(tr.Parent))
                    {
                        const auto& parent_tr = reg.get<TransformComponent>(tr.Parent);
                        local = glm::inverse(parent_tr.WorldMatrix) * model;
                    }

                    if (glm::decompose(local, scale, rotation, translation, skew, perspective))
                    {
                        if (op == ImGuizmo::TRANSLATE)
                            tr.Position = translation;
                        else if (op == ImGuizmo::ROTATE)
                            tr.Rotation = MathUtil::normalizeQuat(rotation);
                        else if (op == ImGuizmo::SCALE)
                            tr.Scale = scale;

                        tr.DirtyLocal = true;
                        ctx.document.activeScene()->MarkDirtyRecursive(
                            Entity(ctx.activeEntity(), &reg, ctx.document.activeScene()));
                        ctx.markSceneDirty();
                    }
                }
            }
        }

        if (drag_state.drag_active && !result.using_gizmo)
        {
            bool committed = false;
            if (ctx.document.activeScene() && drag_state.drag_entity != entt::null)
            {
                auto& reg = ctx.document.activeScene()->getRegistry();
                if (reg.valid(drag_state.drag_entity) && reg.all_of<TransformComponent>(drag_state.drag_entity))
                {
                    const TransformSnapshot after_snapshot =
                        CaptureTransformSnapshot(reg.get<TransformComponent>(drag_state.drag_entity));
                    if (ctx.commands.commit_transform_command)
                    {
                        ctx.commands.commit_transform_command(
                            drag_state.drag_entity, drag_state.drag_before, after_snapshot);
                        committed = true;
                    }
                }
            }

            if (!committed)
            {
                drag_state.drag_entity = entt::null;
                drag_state.drag_before = {};
            }

            drag_state.drag_active = false;
            drag_state.drag_entity = entt::null;
            drag_state.drag_before = {};
        }

        return result;
    }
} // namespace Hybrid
