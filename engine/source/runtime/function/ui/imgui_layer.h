#pragma once
namespace Hybrid {
    class ImGuiLayer : public Layer
    {
    public:
        ImGuiLayer();
        ~ImGuiLayer() override = default;

        void onAttach() override;
        void onDetach() override;
        void onEvent(Event& e) override;       // 可选：用于拦截输入
        void onImGuiRender() override;         // 可选：调试面板

        void begin();
        void end();

        void blockEvents(bool block) { m_BlockEvents = block; }

    private:
        bool m_BlockEvents = true;             // 是否阻止游戏输入
        float m_Time = 0.0f;                   // ImGui 内部时间
    };

} // namespace Hybrid
