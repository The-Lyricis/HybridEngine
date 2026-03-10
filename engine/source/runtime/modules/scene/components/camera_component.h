#pragma once

namespace Hybrid
{
    struct CameraComponent
    {
        bool Primary = false;

        float FovY = 45.0f;
        float Near = 0.1f;
        float Far = 1000.0f;
    };
} // namespace Hybrid
