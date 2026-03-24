#pragma once

#include <vector>

#include "runtime/modules/scene/component_schema.h"
#include "runtime/modules/scene/entity.h"

namespace Hybrid
{
    struct EditorContext;
    using ComponentCustomDrawFn = bool (*)(EditorContext& ctx, Entity entity, void* componentPtr);

    struct ComponentDesc
    {
        const ComponentSchema* schema = nullptr;
        ComponentCustomDrawFn draw_custom = nullptr;

        const char* name = "";
        ComponentFlags flags = ComponentFlags::None;
        ComponentHasFn has = nullptr;
        ComponentAddFn add = nullptr;
        ComponentGetFn get = nullptr;
        ComponentRemoveFn remove = nullptr;
        ComponentEnabledFn enabled = nullptr;
        std::vector<PropertyDesc> properties;
    };

    class ComponentRegistry
    {
    public:
        static const std::vector<ComponentDesc>& GetDescriptors();
    };
} // namespace Hybrid
