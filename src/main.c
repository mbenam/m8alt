#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "common.h"
#include "render.h"
#include "input.h"
#include "backends/m8.h"

int main(int argc, char **argv) {
    struct app_context ctx = {0};
    
    printf("Starting M8C RPi (Framebuffer/Evdev edition)...\n");

    // 1. Initialize Configuration
    // We pass NULL to use default config name, config_read handles pathing
    ctx.conf = config_initialize(NULL);
    config_read(&ctx.conf);

    // 2. Initialize Input (Evdev)
    if (!input_init(&ctx.conf)) {
        fprintf(stderr, "Failed to initialize input. Check permissions or device path.\n");
        return 1;
    }

    // 3. Initialize Renderer (Framebuffer)
    if (!renderer_initialize(&ctx.conf)) {
        fprintf(stderr, "Failed to initialize framebuffer.\n");
        return 1;
    }

    // 4. Initialize M8 Connection (Serial)
    // Retry loop could be added here, but for now we try once
    if (!m8_initialize(1, ctx.preferred_device)) {
        fprintf(stderr, "M8 device not found. Starting in disconnected state.\n");
        ctx.device_connected = 0;
        ctx.app_state = WAIT_FOR_DEVICE;
    } else {
        ctx.device_connected = 1;
        ctx.app_state = RUN;
        m8_enable_display(1);
    }

    printf("Initialization complete. Entering main loop.\n");

    // 5. Main Loop
    while (1) {
        // Poll Input
        input_poll(&ctx);

        // State Machine
        switch (ctx.app_state) {
            case RUN:
                // Read from M8
                if (m8_process_data(&ctx.conf) == DEVICE_DISCONNECTED) {
                    ctx.device_connected = 0;
                    ctx.app_state = WAIT_FOR_DEVICE;
                    renderer_clear_screen();
                }
                break;
            
            case WAIT_FOR_DEVICE:
                // Simple reconnection logic
                usleep(500000); // Wait 0.5s
                if (m8_initialize(0, ctx.preferred_device)) {
                    ctx.device_connected = 1;
                    ctx.app_state = RUN;
                    m8_enable_display(1);
                }
                break;

            case QUIT:
                goto cleanup;
                
            default:
                break;
        }

        // Render Frame
        render_screen(&ctx.conf);

        // Cap to ~60 FPS (16ms)
        usleep(16000);
    }

cleanup:
    printf("Shutting down.\n");
    renderer_close();
    input_close();
    m8_close();
    return 0;
}
