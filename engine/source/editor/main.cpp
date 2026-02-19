#include <iostream>
#include "runtime/engine.h"

int main(int argc, char** argv)
{
    Hybrid::HybridEngine engine;
    engine.initialize();
    engine.run();
    engine.shutdown();
    return 0;
}
