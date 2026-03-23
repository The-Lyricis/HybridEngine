#pragma once

#include <functional>
#include <string>

namespace Hybrid
{
    struct EditorConfirmDialog
    {
        std::string title;
        std::string message;
        std::string confirm_label = "OK";
        std::string secondary_label;
        std::string cancel_label = "Cancel";
        std::function<void()> on_confirm;
        std::function<void()> on_secondary;
        std::function<void()> on_cancel;
    };
} // namespace Hybrid
