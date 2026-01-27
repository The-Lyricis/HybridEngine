#include <iostream>
#include <imgui.h>

#include "runtime/engine.h"
#include "editor/source/editor_ui.h"
#include <GLFW/glfw3.h>

int main(int argc, char** argv)
{
    std::cout << "Starting TDA572 Engine..." << std::endl;
    
    Engine::Engine engine;
	Engine::EditorUI editorUI;

    engine.initialize();
	editorUI.initialize();
    while (!editorUI.isWindowShouldClose()) {

        editorUI.display();
        engine.run();
    }
	editorUI.cleanup();
  
    return 0;
}