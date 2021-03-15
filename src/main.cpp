#include <iostream>
#include <engine/vk_engine.h>


int main() {

    VulkanEngine engine;

    engine.Init();

    engine.Run();

    engine.Cleanup();

    return 0;
}
