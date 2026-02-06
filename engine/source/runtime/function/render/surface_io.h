#pragma once
#include<functional>
#include<GLFW/glfw3.h>

namespace Hybrid
{
	class SurfaceIO
	{
	public:
		//定义输入事件回调函数类型
		using OnKeyFunc = std::function<void(int key, int scancode, int action, int mods)>; //键盘按键（键码、扫描码、动作如按下/释放、修饰键如 Shift/Ctrl）
		using OnCursorPosFunc = std::function<void(double x, double y)>; //鼠标坐标（x, y）
		using OnMouseButtonFunc = std::function<void(int button, int action, int mods)>; //鼠标按键

		explicit SurfaceIO(GLFWwindow* window);
		~SurfaceIO() = default;

		void registerOnKeyFunc(OnKeyFunc f)
		{
			m_on_key = std::move(f);
		}
		void registerOnCursorPosFunc(OnCursorPosFunc f)
		{
			m_on_cursor = std::move(f);
		}
		void registerOnMouseButtonFunc(OnMouseButtonFunc f)
		{
			m_on_mouse_button = std::move(f);
		}

		void setFocusMode(bool enable);
		bool isFocusMode() const
		{
			return m_focus_mode;
		}
		GLFWwindow* getWindow() const
		{
			return m_window;
		}
	private:
		struct CallbackData //辅助结构体，用于在GLFW回调中访问SurfaceIO实例
		{
			SurfaceIO* self;
		};

		void installGlfwCallbacks();

	private:
		GLFWwindow* m_window{ nullptr };
		bool m_focus_mode{ false };
		OnKeyFunc m_on_key;
		OnCursorPosFunc m_on_cursor;
		OnMouseButtonFunc m_on_mouse_button;
		CallbackData m_cb_data;
	};
}