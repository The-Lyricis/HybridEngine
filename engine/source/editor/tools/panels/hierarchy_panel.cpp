#include "hierarchy_panel.h"

#include "editor/core/context/editor_context.h"
#include "editor/core/editor_drag_drop.h"

#include "runtime/core/base/macro.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/scene.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <imgui.h>

namespace Hybrid
{
    namespace
    {
        constexpr const char* kHierarchyPanelLogTag = "[HierarchyPanel]";

        enum class DropIntent : uint8_t
        {
            None = 0,
            AsChild,
            Before,
            After
        };

        bool hasTransform(entt::registry& registry, entt::entity entity)
        {
            return entity != entt::null && registry.valid(entity) && registry.all_of<TransformComponent>(entity);
        }

        const char* getEntityLabel(entt::registry& registry, entt::entity entity)
        {
            if (auto* tag = registry.try_get<TagComponent>(entity))
                return tag->Tag.c_str();
            return "Entity";
        }

        uint32_t entityHandleValue(entt::entity entity)
        {
            return entt::to_integral(entity);
        }

        DropIntent calcDropIntent(const ImVec2& min, const ImVec2& max, float mouseY)
        {
            const float height = std::max(1.0f, max.y - min.y);
            const float topBand = min.y + height * 0.25f;
            const float bottomBand = min.y + height * 0.75f;

            if (mouseY < topBand)
                return DropIntent::Before;
            if (mouseY > bottomBand)
                return DropIntent::After;
            return DropIntent::AsChild;
        }

        void drawDropPreview(const ImVec2& min, const ImVec2& max, DropIntent intent)
        {
            auto* drawList = ImGui::GetWindowDrawList();
            const ImU32 color = ImGui::GetColorU32(ImGuiCol_DragDropTarget);

            if (intent == DropIntent::Before || intent == DropIntent::After)
            {
                const float y = (intent == DropIntent::Before) ? min.y : max.y;
                drawList->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), color, 2.0f);
                return;
            }

            if (intent == DropIntent::AsChild)
                drawList->AddRect(min, max, color, 0.0f, 0, 1.0f);
        }

    } // namespace

    void HierarchyPanel::queueAction(PendingActionType type, entt::entity target)
    {
        m_pendingAction = type;
        m_pendingTarget = target;
    }

    void HierarchyPanel::collectEntityOrder(entt::registry& registry,
                                            entt::entity entity,
                                            std::vector<entt::entity>& order) const
    {
        if (!hasTransform(registry, entity))
            return;

        order.push_back(entity);

        const auto& transform = registry.get<TransformComponent>(entity);
        for (entt::entity child = transform.FirstChild; child != entt::null;)
        {
            if (!hasTransform(registry, child))
                break;

            const entt::entity next = registry.get<TransformComponent>(child).NextSibling;
            collectEntityOrder(registry, child, order);
            child = next;
        }
    }

    void HierarchyPanel::applySelectionClick(EditorContext& ctx, entt::entity entity)
    {
        if (entity == entt::null)
        {
            ctx.selection.clear();
            return;
        }

        const ImGuiIO& io = ImGui::GetIO();
        const bool ctrl = io.KeyCtrl;
        const bool shift = io.KeyShift;

        if (shift && ctx.selection.rangeAnchor() != entt::null && !m_visibleOrder.empty())
        {
            const entt::entity anchor = ctx.selection.rangeAnchor();
            auto anchor_it = std::find(m_visibleOrder.begin(), m_visibleOrder.end(), anchor);
            auto target_it = std::find(m_visibleOrder.begin(), m_visibleOrder.end(), entity);
            if (anchor_it != m_visibleOrder.end() && target_it != m_visibleOrder.end())
            {
                if (!ctrl)
                    ctx.selection.clear();

                if (anchor_it > target_it)
                    std::swap(anchor_it, target_it);

                for (auto it = anchor_it; it != target_it + 1; ++it)
                {
                    if (!ctx.selection.contains(*it))
                        ctx.selection.items().push_back(*it);
                }

                ctx.selection.setActive(entity);
                ctx.selection.setRangeAnchor(anchor);
                return;
            }
        }

        if (ctrl)
        {
            ctx.selection.toggle(entity);
            return;
        }

        ctx.selection.setSingle(entity);
    }

    void HierarchyPanel::drawCommonContextMenu(EditorContext& ctx, entt::registry& registry, entt::entity target)
    {
        const bool has_target = hasTransform(registry, target);
        const bool has_parent =
            has_target && registry.get<TransformComponent>(target).Parent != entt::null;

        if (ImGui::MenuItem("Delete", nullptr, false, has_target))
            queueAction(PendingActionType::Delete, target);

        if (ImGui::MenuItem("Duplicate", nullptr, false, has_target))
            queueAction(PendingActionType::Duplicate, target);

        if (ImGui::MenuItem("Unparent", nullptr, false, has_parent))
            queueAction(PendingActionType::Unparent, target);

        ImGui::Separator();

        if (ImGui::MenuItem("Create Empty"))
            queueAction(PendingActionType::CreateRootEmpty, target);
        if (ImGui::BeginMenu("3D Object"))
        {
            const bool can_create_cube =
                ctx.scene_actions.get_builtin_mesh_id && ctx.scene_actions.get_builtin_mesh_id(BuiltinMesh::Cube).value != 0;
            if (ImGui::MenuItem("Cube", nullptr, false, can_create_cube))
                queueAction(PendingActionType::CreateRootCube, target);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Camera"))
            queueAction(PendingActionType::CreateRootCamera, target);
        if (ImGui::BeginMenu("Light"))
        {
            if (ImGui::MenuItem("Directional Light"))
                queueAction(PendingActionType::CreateRootDirectionalLight, target);
            if (ImGui::MenuItem("Point Light"))
                queueAction(PendingActionType::CreateRootPointLight, target);
            ImGui::EndMenu();
        }

        if (has_target)
            ctx.selection.setSingle(target);
    }

    void HierarchyPanel::drawWindowContextMenu(EditorContext& ctx)
    {
        if (!ctx.document.activeScene())
            return;

        auto& registry = ctx.document.activeScene()->getRegistry();
        drawCommonContextMenu(ctx, registry, entt::null);
    }

    void HierarchyPanel::drawEntityContextMenu(EditorContext& ctx, entt::registry& registry, entt::entity entity)
    {
        if (!ImGui::BeginPopupContextItem())
            return;

        drawCommonContextMenu(ctx, registry, entity);
        ImGui::EndPopup();
    }

    void HierarchyPanel::flushPendingAction(EditorContext& ctx, entt::registry& registry)
    {
        if (!ctx.document.activeScene() || m_pendingAction == PendingActionType::None)
            return;

        auto reset = [this]()
        {
            m_pendingAction = PendingActionType::None;
            m_pendingTarget = entt::null;
        };

        switch (m_pendingAction)
        {
        case PendingActionType::Delete:
        {
            if (!registry.valid(m_pendingTarget) || !ctx.scene_actions.delete_entity)
            {
                HBD_CORE_WARN("{} delete_rejected entity={} reason=invalid_entity",
                              kHierarchyPanelLogTag,
                              entityHandleValue(m_pendingTarget));
                break;
            }

            const std::string entity_name = getEntityLabel(registry, m_pendingTarget);
            if (ctx.scene_actions.delete_entity(m_pendingTarget))
            {
                HBD_CORE_INFO("{} delete_completed entity={} name={}",
                              kHierarchyPanelLogTag,
                              entityHandleValue(m_pendingTarget),
                              entity_name);
            }
            else
            {
                HBD_CORE_WARN("{} delete_failed entity={} name={}",
                              kHierarchyPanelLogTag,
                              entityHandleValue(m_pendingTarget),
                              entity_name);
            }
            break;
        }
        case PendingActionType::Duplicate:
        {
            if (!registry.valid(m_pendingTarget) || !ctx.scene_actions.duplicate_selection)
            {
                HBD_CORE_WARN("{} duplicate_rejected entity={} reason=invalid_entity",
                              kHierarchyPanelLogTag,
                              entityHandleValue(m_pendingTarget));
                break;
            }

            if (ctx.scene_actions.duplicate_selection(m_pendingTarget))
            {
                HBD_CORE_INFO("{} duplicate_completed entity={} name={}",
                              kHierarchyPanelLogTag,
                              entityHandleValue(m_pendingTarget),
                              getEntityLabel(registry, m_pendingTarget));
            }
            else
            {
                HBD_CORE_WARN("{} duplicate_failed entity={} name={}",
                              kHierarchyPanelLogTag,
                              entityHandleValue(m_pendingTarget),
                              getEntityLabel(registry, m_pendingTarget));
            }
            break;
        }
        case PendingActionType::Unparent:
        {
            if (!hasTransform(registry, m_pendingTarget))
            {
                HBD_CORE_WARN("{} unparent_rejected entity={} reason=missing_transform",
                              kHierarchyPanelLogTag,
                              entityHandleValue(m_pendingTarget));
                break;
            }

            Entity child{m_pendingTarget, &registry, ctx.document.activeScene()};
            if (ctx.document.activeScene()->Detach(child, true))
            {
                ctx.selection.setSingle(m_pendingTarget);
                ctx.markSceneDirty();
                HBD_CORE_INFO("{} unparent_completed entity={} name={}",
                              kHierarchyPanelLogTag,
                              entityHandleValue(m_pendingTarget),
                              getEntityLabel(registry, m_pendingTarget));
            }
            else
            {
                HBD_CORE_WARN("{} unparent_failed entity={} name={}",
                              kHierarchyPanelLogTag,
                              entityHandleValue(m_pendingTarget),
                              getEntityLabel(registry, m_pendingTarget));
            }
            break;
        }
        case PendingActionType::CreateRootEmpty:
        {
            const bool parent_requested = hasTransform(registry, m_pendingTarget);
            const entt::entity created =
                ctx.scene_actions.create_entity ? ctx.scene_actions.create_entity(SceneEntityTemplate::Empty,
                                                                  parent_requested ? m_pendingTarget : entt::null)
                                        : entt::null;
            if (created != entt::null)
            {
                HBD_CORE_INFO("{} create_completed entity={} name=Empty parent_entity={}",
                              kHierarchyPanelLogTag,
                              entityHandleValue(created),
                              parent_requested ? std::to_string(entityHandleValue(m_pendingTarget)) : std::string("<root>"));
            }
            break;
        }
        case PendingActionType::CreateRootCube:
        {
            const bool parent_requested = hasTransform(registry, m_pendingTarget);
            const entt::entity created =
                ctx.scene_actions.create_entity ? ctx.scene_actions.create_entity(SceneEntityTemplate::Cube,
                                                                  parent_requested ? m_pendingTarget : entt::null)
                                        : entt::null;
            if (created != entt::null)
            {
                HBD_CORE_INFO("{} create_completed entity={} name=Cube parent_entity={}",
                              kHierarchyPanelLogTag,
                              entityHandleValue(created),
                              parent_requested ? std::to_string(entityHandleValue(m_pendingTarget)) : std::string("<root>"));
            }
            else
            {
                HBD_CORE_WARN("{} create_rejected entity_type=Cube reason=create_scene_entity_failed",
                              kHierarchyPanelLogTag);
            }
            break;
        }
        case PendingActionType::CreateRootCamera:
        {
            const bool parent_requested = hasTransform(registry, m_pendingTarget);
            const entt::entity created =
                ctx.scene_actions.create_entity ? ctx.scene_actions.create_entity(SceneEntityTemplate::Camera,
                                                                  parent_requested ? m_pendingTarget : entt::null)
                                        : entt::null;
            if (created != entt::null)
            {
                HBD_CORE_INFO("{} create_completed entity={} name=Camera parent_entity={}",
                              kHierarchyPanelLogTag,
                              entityHandleValue(created),
                              parent_requested ? std::to_string(entityHandleValue(m_pendingTarget)) : std::string("<root>"));
            }
            break;
        }
        case PendingActionType::CreateRootDirectionalLight:
        {
            const bool parent_requested = hasTransform(registry, m_pendingTarget);
            const entt::entity created =
                ctx.scene_actions.create_entity ? ctx.scene_actions.create_entity(SceneEntityTemplate::DirectionalLight,
                                                                  parent_requested ? m_pendingTarget : entt::null)
                                        : entt::null;
            if (created != entt::null)
            {
                HBD_CORE_INFO("{} create_completed entity={} name=\"Directional Light\" parent_entity={}",
                              kHierarchyPanelLogTag,
                              entityHandleValue(created),
                              parent_requested ? std::to_string(entityHandleValue(m_pendingTarget)) : std::string("<root>"));
            }
            break;
        }
        case PendingActionType::CreateRootPointLight:
        {
            const bool parent_requested = hasTransform(registry, m_pendingTarget);
            const entt::entity created =
                ctx.scene_actions.create_entity ? ctx.scene_actions.create_entity(SceneEntityTemplate::PointLight,
                                                                  parent_requested ? m_pendingTarget : entt::null)
                                        : entt::null;
            if (created != entt::null)
            {
                HBD_CORE_INFO("{} create_completed entity={} name=\"Point Light\" parent_entity={}",
                              kHierarchyPanelLogTag,
                              entityHandleValue(created),
                              parent_requested ? std::to_string(entityHandleValue(m_pendingTarget)) : std::string("<root>"));
            }
            break;
        }
        case PendingActionType::None:
            break;
        }

        reset();
    }

    void HierarchyPanel::drawEntityNode(EditorContext& ctx,
                                        entt::registry& registry,
                                        entt::entity entity,
                                        std::unordered_set<uint32_t>& visited)
    {
        if (!hasTransform(registry, entity))
            return;

        const uint32_t raw = entt::to_integral(entity);
        if (!visited.insert(raw).second)
            return;

        const auto& transform = registry.get<TransformComponent>(entity);

        const bool hasChildren = transform.FirstChild != entt::null && hasTransform(registry, transform.FirstChild);
        const bool selected = ctx.selection.contains(entity);

        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_SpanAvailWidth |
            (selected ? ImGuiTreeNodeFlags_Selected : 0);

        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        const auto id = reinterpret_cast<void*>(static_cast<uintptr_t>(raw));
        const bool opened = ImGui::TreeNodeEx(id, flags, "%s", getEntityLabel(registry, entity));

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            applySelectionClick(ctx, entity);

        drawEntityContextMenu(ctx, registry, entity);

        EditorDragDrop::BeginDragEntity(entity, getEntityLabel(registry, entity));

        if (ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload(EditorDragDrop::ENTITY, ImGuiDragDropFlags_AcceptBeforeDelivery);
            if (payload && payload->DataSize == sizeof(EditorDragDrop::EntityPayload))
            {
                const auto* p = static_cast<const EditorDragDrop::EntityPayload*>(payload->Data);
                const entt::entity dropped = p->handle;

                if (dropped != entity && hasTransform(registry, dropped))
                {
                    const ImVec2 itemMin = ImGui::GetItemRectMin();
                    const ImVec2 itemMax = ImGui::GetItemRectMax();
                    const DropIntent intent = calcDropIntent(itemMin, itemMax, ImGui::GetIO().MousePos.y);

                    drawDropPreview(itemMin, itemMax, intent);

                    if (payload->IsDelivery())
                    {
                        Entity child{dropped, &registry, ctx.document.activeScene()};
                        bool ok = false;

                        if (intent == DropIntent::AsChild)
                        {
                            Entity parent{entity, &registry, ctx.document.activeScene()};
                            ok = ctx.document.activeScene()->SetParent(child, parent, true);
                        }
                        else
                        {
                            // TODO: Replace with MoveBefore/MoveAfter when sibling reorder APIs are ready.
                            Entity parent{};
                            if (hasTransform(registry, transform.Parent))
                                parent = Entity{transform.Parent, &registry, ctx.document.activeScene()};
                            ok = ctx.document.activeScene()->SetParent(child, parent, true);
                        }

                        if (ok)
                        {
                            ctx.selection.setSingle(dropped);
                            ctx.markSceneDirty();
                            HBD_CORE_INFO("{} reparent_completed entity={} target_entity={} intent={}",
                                          kHierarchyPanelLogTag,
                                          entityHandleValue(dropped),
                                          entityHandleValue(entity),
                                          intent == DropIntent::AsChild
                                              ? "as_child"
                                              : (intent == DropIntent::Before ? "before" : "after"));
                        }
                        else
                        {
                            HBD_CORE_WARN("{} reparent_failed entity={} target_entity={} intent={}",
                                          kHierarchyPanelLogTag,
                                          entityHandleValue(dropped),
                                          entityHandleValue(entity),
                                          intent == DropIntent::AsChild
                                              ? "as_child"
                                              : (intent == DropIntent::Before ? "before" : "after"));
                        }
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }

        if (!opened || !hasChildren)
            return;

        for (entt::entity child = transform.FirstChild; child != entt::null;)
        {
            if (!hasTransform(registry, child))
                break;

            const entt::entity next = registry.get<TransformComponent>(child).NextSibling;
            drawEntityNode(ctx, registry, child, visited);
            child = next;
        }

        ImGui::TreePop();
    }

    void HierarchyPanel::drawRootDropTarget(EditorContext& ctx, entt::registry& registry)
    {
        ImVec2 size{ImGui::GetContentRegionAvail().x, 28.0f};
        if (size.x < 1.0f)
            size.x = 1.0f;
        ImGui::InvisibleButton("##HierarchyRootDropTarget", size);

        if (ImGui::BeginDragDropTarget())
        {
            entt::entity dropped = entt::null;
            if (EditorDragDrop::AcceptEntity(dropped) && hasTransform(registry, dropped))
            {
                Entity child{dropped, &registry, ctx.document.activeScene()};
                if (ctx.document.activeScene()->Detach(child, true))
                {
                    ctx.selection.setSingle(dropped);
                    ctx.markSceneDirty();
                    HBD_CORE_INFO("{} reparent_to_root_completed entity={}",
                                  kHierarchyPanelLogTag,
                                  entityHandleValue(dropped));
                }
                else
                {
                    HBD_CORE_WARN("{} reparent_to_root_failed entity={}",
                                  kHierarchyPanelLogTag,
                                  entityHandleValue(dropped));
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    void HierarchyPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_state.open)
            return;

        ImGui::Begin(getName(), &m_state.open);

        if (!ctx.document.activeScene())
        {
            ImGui::TextDisabled("No active scene.");
            ImGui::End();
            return;
        }

        auto& registry = ctx.document.activeScene()->getRegistry();

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
            applySelectionClick(ctx, entt::null);

        const auto roots = ctx.document.activeScene()->getRootEntities();
        m_visibleOrder.clear();
        for (const Entity& root : roots)
            collectEntityOrder(registry, root.GetHandle(), m_visibleOrder);

        std::unordered_set<uint32_t> visited;
        visited.reserve(static_cast<size_t>(registry.view<TransformComponent>().size() + 8));

        for (const Entity& root : roots)
            drawEntityNode(ctx, registry, root.GetHandle(), visited);

        drawRootDropTarget(ctx, registry);
        drawWindowContextMenuIfRequested(ctx);
        flushPendingAction(ctx, registry);

        ImGui::End();
    }
} // namespace Hybrid


