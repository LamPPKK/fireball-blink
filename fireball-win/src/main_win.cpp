#include "fireball/win/app_window.h"
#include <iostream>

#if defined(_WIN32)
#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)pCmdLine;
    (void)nCmdShow;

    fireball::win::AppWindow app;
    if (!app.Initialize()) {
        MessageBoxW(nullptr, L"Failed to initialize Fireball WebView2 host.", L"Fireball Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    app.Run();
    return 0;
}
#else
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "Fireball Lite for Windows (Cross-Platform Mock Runner)\n";
    fireball::win::AppWindow app;
    if (!app.Initialize()) {
        std::cerr << "Initialization failed\n";
        return 1;
    }
    std::cout << "Active Space: " << app.GetActiveSpace().name << "\n";
    std::cout << "Active Tab URL: " << app.GetActiveTab()->url << "\n";
    return 0;
}
#endif
