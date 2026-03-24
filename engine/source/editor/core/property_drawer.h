#pragma once

#include "editor/core/component_value_command.h"
#include "editor/core/component_registry.h"
#include "editor/core/editor_context.h"

namespace Hybrid
{
    bool DrawPropertyField(EditorContext* ctx,
                           Entity entity,
                           void* componentPtr,
                           const PropertyDesc& property);

    inline bool DrawPropertyField(void* componentPtr, const PropertyDesc& property)
    {
        return DrawPropertyField(nullptr, {}, componentPtr, property);
    }

    template<typename TComponent>
    bool DrawTrackedPropertyField(EditorContext& ctx,
                                  Entity entity,
                                  TComponent& component,
                                  const PropertyDesc& property)
    {
        return DrawTrackedContinuousComponentEdit(ctx,
                                                  entity,
                                                  component,
                                                  property.label(),
                                                  [&]()
                                                  {
                                                      return DrawPropertyField(&ctx, entity, &component, property);
                                                  });
    }
} // namespace Hybrid
