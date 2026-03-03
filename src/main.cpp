#include "windows_resize_app.hpp"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    WindowsResizeApp app(instance);
    return app.Run();
}

