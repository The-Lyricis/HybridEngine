#include <iostream>
#include <imgui.h>

#include "runtime/engine.h"
#include "editor/include/editor_ui.h"
#include"runtime/function/input/input_system.h"
#include"runtime/core/log/log_system.h"
#include "runtime/function/window/window_system.h"
#include <GLFW/glfw3.h>

static void TestInputLogging();

int main(int argc, char** argv)
{
    std::cout << "Starting Hybrid Engine..." << std::endl;
    
    Hybrid::HybridEngine engine;
	
	Hybrid::LogSystem::Init();

	std::shared_ptr<Hybrid::WindowSystem> window_system = std::make_shared<Hybrid::WindowSystem>(); //创建窗口系统实例
	window_system->initialize(1280, 720, "Hybrid Engine Editor");
	Hybrid::InputSystem inputSystem = Hybrid::InputSystem::get(); //获取输入系统单例
    inputSystem.initialize(*window_system->getSurfaceIO());

	auto editor_ui = std::make_shared<Hybrid::EditorUI>(); //创建编辑器UI实例
    editor_ui->initialize(window_system->getGLFWWindow());

	

    engine.initialize();
	
    
    HBD_CORE_INFO("Hybrid Engine Initialized.");
    HBD_INFO("Hybrid Engine Client Initialized.");
    while (!window_system->shouldClose()) {
        window_system->pollEvents();
        editor_ui->display();
        engine.run();


        //if (!inputSystem.isInputValid())
        //    HBD_INFO("输入初始化失败\n");

        //// 边沿测试：按下 A 应只打印一次
        //if (inputSystem.wasKeyPressed(GLFW_KEY_A))
        //    HBD_INFO("A Pressed (edge) OK");

        //// 持续测试：按住 W 时会持续触发
        //if (inputSystem.isKeyDown(GLFW_KEY_W))
        //    HBD_INFO("W Down (hold) OK");

        //// 鼠标增量测试：移动鼠标应有变化（focus mode 下）
        //const double dx = inputSystem.getMouseDeltaX();
        //const double dy = inputSystem.getMouseDeltaY();
        //if (dx != 0.0 || dy != 0.0)
        //    HBD_INFO("Mouse delta: dx={}, dy={}", dx, dy);


    }
    window_system->cleanup();
    Hybrid::LogSystem::Shutdown();

    return 0;
}

