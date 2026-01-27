#pragma once
namespace Engine {
	class WindowSystem {
	public:
		void initialize();
		void createWindow(int width, int height, const char* title);
	};
} // namespace Engine