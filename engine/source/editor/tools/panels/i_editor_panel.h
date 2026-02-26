#pragma once

namespace Hybrid
{
    struct EditorContext;

    class IEditorPanel
    {
    public:
        virtual ~IEditorPanel() = default;

        // 面板窗口标题（必须与 DockBuilderDockWindow 的字符串一致）
        virtual const char* getName() const = 0;

        // 渲染 UI
        virtual void onImGuiRender(EditorContext& ctx) = 0;

        bool isOpen() const { return m_open; }
        void setOpen(bool open) { m_open = open; }

    protected:
        bool m_open = true; // 是否显示
    };
} // namespace Hybrid
