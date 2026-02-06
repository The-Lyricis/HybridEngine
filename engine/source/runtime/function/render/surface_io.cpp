#include<runtime/function/render/surface_io.h>
namespace Hybrid
{
	SurfaceIO::SurfaceIO(GLFWwindow* window)
		: m_window(window)
	{
		m_cb_data.self = this;
		glfwSetWindowUserPointer(m_window, &m_cb_data);
		installGlfwCallbacks();
	}
	void SurfaceIO::installGlfwCallbacks()
	{
		glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
			auto* cb_data = static_cast<CallbackData*>(glfwGetWindowUserPointer(window));
			if (cb_data && cb_data->self && cb_data->self->m_on_key)
				cb_data->self->m_on_key(key, scancode, action, mods);
			});
		glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double x, double y) {
			auto* cb_data = static_cast<CallbackData*>(glfwGetWindowUserPointer(window));
			if (cb_data && cb_data->self && cb_data->self->m_on_cursor)
				cb_data->self->m_on_cursor(x, y);
			});
		glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
			auto* cb_data = static_cast<CallbackData*>(glfwGetWindowUserPointer(window));
			if (cb_data && cb_data->self && cb_data->self->m_on_mouse_button)
				cb_data->self->m_on_mouse_button(button, action, mods);
			});
	}
	void SurfaceIO::setFocusMode(bool enable)
	{
		m_focus_mode = enable;
		glfwSetInputMode(m_window, GLFW_CURSOR, enable ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	}
}