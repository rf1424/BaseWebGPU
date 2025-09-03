#include "Application.h"

// Emscripten
#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

int main() {
    Application app;

    if (!app.Initialize()) {
        return 1;
    }

#ifdef __EMSCRIPTEN__ // TODO for web come back
    // Equivalent of the main loop when using Emscripten:
    auto callback = [](void* arg) {
        //                   ^^^ 2. We get the address of the app in the callback.
        Application* pApp = reinterpret_cast<Application*>(arg);
        //                  ^^^^^^^^^^^^^^^^ 3. We force this address to be interpreted
        //                                      as a pointer to an Application object.
        pApp->MainLoop(); // 4. We can use the application object
        };
    emscripten_set_main_loop_arg(callback, &app, 0, true);
    //                                     ^^^^ 1. We pass the address of our application object.
#else 
    while (app.IsRunning()) {
        app.MainLoop();
    }
#endif

    app.Terminate();

    return 0;
}

