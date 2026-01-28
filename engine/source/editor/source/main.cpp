#include <iostream>
#include <imgui.h>

#include "runtime/engine.h"
#include "editor/include/editor_ui.h"

#include"runtime/core/log/log_system.h"

int main(int argc, char** argv)
{
    std::cout << "Starting TDA572 Engine..." << std::endl;
    
    Hybrid::HybridEngine engine;
	Hybrid::EditorUI editorUI;
	Hybrid::LogSystem::Init();

	

    engine.initialize();
	editorUI.initialize();
    HBD_CORE_INFO("Hybrid Engine Initialized.");
    HBD_INFO("Hybrid Engine Client Initialized.");
    while (!editorUI.isWindowShouldClose()) {

        editorUI.display();
        engine.run();
    }
	editorUI.cleanup();
    Hybrid::LogSystem::Shutdown();

    return 0;
}