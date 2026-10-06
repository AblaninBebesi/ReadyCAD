#include "readycad/ui/Application.hpp"

#include <cstdio>
#include <exception>

int main()
{
    try {
        rc::ui::Application app;
        return app.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ReadyCAD: fatal error: %s\n", e.what());
        return 1;
    }
}
