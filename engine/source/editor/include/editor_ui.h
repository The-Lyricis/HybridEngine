#include <GLFW/glfw3.h>

namespace Hybrid {
	class EditorUI {
	public:
		void initialize();
		void display();
		bool isWindowShouldClose();
		void cleanup();
		GLFWwindow* window;
	};
}