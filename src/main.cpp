#include "app/App.h"
#include "util/Logger.h"

#include <whb/proc.h>

int main(int argc, char* argv[]) {
    WHBProcInit();

    App app;

    if (!app.init()) {
        LOG_ERROR("Failed to initialize app");
        WHBProcShutdown();
        return 1;
    }

    app.run();

    WHBProcShutdown();
    return 0;
}
