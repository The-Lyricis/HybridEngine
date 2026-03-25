#pragma once

#include <functional>
#include <memory>

#include "runtime/modules/render/runtime/editor_render_ext.h"
#include "runtime/modules/render/runtime/frame_view_data.h"
#include "runtime/modules/render/runtime/frame_context.h"
#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/runtime/render_packet.h"

namespace Hybrid
{
    class Scene;

    struct FrameViewResolveInput
    {
        std::shared_ptr<Scene> scene;
        const FrameContext* frame = nullptr;
        const EditorRenderExt* editor_ext = nullptr;
        RenderFlags flags = RenderFlags::None;
        std::function<TexturePtr(AssetID)> resolve_cubemap;
    };

    struct FrameViewResolveResult
    {
        FrameViewData view;
        RenderEnvironmentData environment;
        bool has_camera = false;
    };

    class FrameViewResolver
    {
    public:
        FrameViewResolveResult resolve(const FrameViewResolveInput& input) const;
    };
} // namespace Hybrid
