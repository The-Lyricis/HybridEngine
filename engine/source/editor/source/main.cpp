#include <iostream>

#include "runtime/engine.h"

int main(int argc, char** argv)
{
    std::cout << "Starting TDA572 Engine..." << std::endl;
    
    Engine::Engine engine;
    engine.initialize();
    engine.run();
    return 0;
}