#include "hierarchy_panel.h"

#include "editor/core/editor_context.h"
#include "editor/core/editor_drag_drop.h"

#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/scene.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include <entt/entt.hpp>
#include <imgui.h>

namespace Hybrid
{
    namespace
    {
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
        const bool selected = (ctx.selected == entity);

        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_SpanAvailWidth |
            (selected ? ImGuiTreeNodeFlags_Selected : 0);

        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        const auto id = reinterpret_cast<void*>(static_cast<uintptr_t>(raw));
        const bool opened = ImGui::TreeNodeEx(id, flags, "%s", getEntityLabel(registry, entity));

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            ctx.selected = entity;

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
                        Entity child{dropped, &registry, ctx.active_scene};
                        bool ok = false;

                        if (intent == DropIntent::AsChild)
                        {
                            Entity parent{entity, &registry, ctx.active_scene};
                            ok = ctx.active_scene->SetParent(child, parent, true);
                        }
                        else
                        {
                            // TODO: Replace with MoveBefore/MoveAfter when sibling reorder APIs are ready.
                            Entity parent{};
                            if (hasTransform(registry, transform.Parent))
                                parent = Entity{transform.Parent, &registry, ctx.active_scene};
                            ok = ctx.active_scene->SetParent(child, parent, true);
                        }

                        if (ok)
                            ctx.selected = dropped;
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
        ImGui::Separator();
        ImGui::TextDisabled("Drop entity here to detach to root");

        ImVec2 size{ImGui::GetContentRegionAvail().x, 28.0f};
        if (size.x < 1.0f)
            size.x = 1.0f;
        ImGui::InvisibleButton("##HierarchyRootDropTarget", size);

        if (ImGui::BeginDragDropTarget())
        {
            entt::entity dropped = entt::null;
            if (EditorDragDrop::AcceptEntity(dropped) && hasTransform(registry, dropped))
            {
                Entity child{dropped, &registry, ctx.active_scene};
                if (ctx.active_scene->Detach(child, true))
                    ctx.selected = dropped;
            }
            ImGui::EndDragDropTarget();
        }
    }

    void HierarchyPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_open)
            return;

        ImGui::Begin(getName(), &m_open);

        if (!ctx.active_scene)
        {
            ImGui::TextDisabled("No active scene.");
            ImGui::End();
            return;
        }

        auto& registry = ctx.active_scene->getRegistry();

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
            ctx.selected = entt::null;

        auto transformView = registry.view<TransformComponent>();
        std::vector<entt::entity> roots;
        roots.reserve(transformView.size());

        for (entt::entity entity : transformView)
        {
            const auto& tc = transformView.get<TransformComponent>(entity);
            if (tc.Parent == entt::null || !hasTransform(registry, tc.Parent))
                roots.push_back(entity);
        }

        std::sort(roots.begin(), roots.end(), [](entt::entity a, entt::entity b) {
            return entt::to_integral(a) < entt::to_integral(b);
        });

        std::unordered_set<uint32_t> visited;
        visited.reserve(static_cast<size_t>(transformView.size() + 8));

        for (entt::entity root : roots)
            drawEntityNode(ctx, registry, root, visited);

        drawRootDropTarget(ctx, registry);

        ImGui::End();
    }
} // namespace Hybrid


