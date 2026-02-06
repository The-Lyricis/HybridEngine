#include <GLFW/glfw3.h>

namespace Hybrid {
	class EditorUI {
	public:
		void initialize();
		void initialize(GLFWwindow* window);
		void display();
		bool isWindowShouldClose();
		void cleanup();
		GLFWwindow* m_window;
	};
}