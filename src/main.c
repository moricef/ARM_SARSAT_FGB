#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "t001_protocol.h"
#include "biphase_modulator.h"
#include "pluto_control.h"
#include "gpio_control.h"

// =============================
// Global Variables
// =============================

static volatile bool running = true;
static pluto_ctx_t *pluto = NULL;

// =============================
// Signal Handler
// =============================

void signal_handler(int signum) {
    (void)signum;
    printf("\nShutdown signal received...\n");
    running = false;
}

// =============================
// Configuration
// =============================

typedef struct {
    double latitude;
    double longitude;
    double altitude;
    uint32_t beacon_id;
    uint64_t frequency;
    int tx_gain_db;
    uint8_t mode;  // 0 = exercise, 1 = test
    int tx_interval_sec;
} app_config_t;

// Default configuration (France, training mode)
app_config_t default_config = {
    .latitude = 42.95463,        // Test location
    .longitude = 1.364479,
    .altitude = 1080,
    .beacon_id = 0x123456,       // Example beacon ID
    .frequency = 403000000ULL,   // 403 MHz (training)
    .tx_gain_db = -10,           // Low power for training
    .mode = 0,                   // Exercise mode
    .tx_interval_sec = 60        // 1 transmission per minute
};

// =============================
// Main Application
// =============================

void print_usage(const char *progname) {
    printf("COSPAS-SARSAT T.001 Beacon Transmitter\n");
    printf("Usage: %s [options]\n\n", progname);
    printf("Options:\n");
    printf("  -f <freq>     Frequency in Hz (default: 403000000)\n");
    printf("  -g <gain>     TX gain in dB (default: -10)\n");
    printf("  -i <id>       Beacon ID in hex (default: 0x123456)\n");
    printf("  -m <mode>     Mode: 0=exercise, 1=test (default: 0)\n");
    printf("  -t <sec>      TX interval in seconds (default: 60)\n");
    printf("  -lat <lat>    Latitude (default: 42.95463)\n");
    printf("  -lon <lon>    Longitude (default: 1.364479)\n");
    printf("  -alt <alt>    Altitude in meters (default: 1080)\n");
    printf("  -h            Show this help\n\n");
    printf("Example:\n");
    printf("  %s -f 403000000 -g -10 -m 0 -t 120\n", progname);
}

int parse_args(int argc, char *argv[], app_config_t *config) {
    *config = default_config;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            config->frequency = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-g") == 0 && i + 1 < argc) {
            config->tx_gain_db = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            config->beacon_id = strtoul(argv[++i], NULL, 16);
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            config->mode = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            config->tx_interval_sec = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-lat") == 0 && i + 1 < argc) {
            config->latitude = atof(argv[++i]);
        } else if (strcmp(argv[i], "-lon") == 0 && i + 1 < argc) {
            config->longitude = atof(argv[++i]);
        } else if (strcmp(argv[i], "-alt") == 0 && i + 1 < argc) {
            config->altitude = atof(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return -1;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }

    return 0;
}

void print_config(const app_config_t *config) {
    printf("\n=== T.001 Beacon Configuration ===\n");
    printf("Beacon ID:    0x%06X\n", config->beacon_id);
    printf("Position:     %.6f, %.6f\n", config->latitude, config->longitude);
    printf("Altitude:     %.0f m\n", config->altitude);
    printf("Frequency:    %llu Hz (%.3f MHz)\n",
           (unsigned long long)config->frequency, config->frequency / 1e6);
    printf("TX Gain:      %d dB\n", config->tx_gain_db);
    printf("Mode:         %s\n", config->mode ? "TEST" : "EXERCISE");
    printf("TX Interval:  %d seconds\n", config->tx_interval_sec);
    printf("==================================\n\n");
}

int main(int argc, char *argv[]) {
    app_config_t config;
    uint8_t frame[MESSAGE_BITS];
    beacon_config_t beacon_cfg;

    // Install signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Parse command line arguments
    if (parse_args(argc, argv, &config) < 0) {
        return 1;
    }

    print_config(&config);

    // Initialize GPIO
    if (!gpio_init()) {
        fprintf(stderr, "GPIO initialization failed\n");
        return 1;
    }

    // Turn on status LED
    gpio_status_led(true);

    // Initialize PlutoSDR
    pluto = pluto_init(PLUTO_URI);
    if (!pluto) {
        fprintf(stderr, "PlutoSDR initialization failed\n");
        gpio_cleanup();
        return 1;
    }

    // Build beacon configuration
    beacon_cfg.latitude = config.latitude;
    beacon_cfg.longitude = config.longitude;
    beacon_cfg.altitude = config.altitude;
    beacon_cfg.beacon_id = config.beacon_id;
    beacon_cfg.mode = config.mode;

    // Build T.001 frame
    printf("Building T.001 frame...\n");
    build_t001_frame(frame, &beacon_cfg);

    // Validate frame
    if (!validate_t001_frame(frame)) {
        fprintf(stderr, "ERROR: Frame validation failed!\n");
        pluto_cleanup(pluto);
        gpio_cleanup();
        return 1;
    }

    printf("Frame validation: PASS\n");
    print_frame_hex(frame);
    print_frame_analysis(frame);

    // Configure TX once (not per transmission)
    printf("Configuring TX...\n");
    if (!pluto_configure_tx(pluto, config.frequency, PLUTO_SAMPLE_RATE, config.tx_gain_db)) {
        fprintf(stderr, "TX configuration failed\n");
        pluto_cleanup(pluto);
        gpio_cleanup();
        return 1;
    }

    // Main transmission loop
    printf("\nStarting transmission loop (Ctrl+C to stop)...\n");

    int tx_count = 0;
    while (running) {
        printf("\n--- Transmission #%d ---\n", ++tx_count);

        // Prepare for TX (PA, relay, LEDs)
        if (!gpio_prepare_tx()) {
            fprintf(stderr, "Failed to prepare TX\n");
            break;
        }

        // Transmit T.001 frame (TX already configured)
        if (!pluto_transmit_t001_frame_simple(pluto, frame)) {
            fprintf(stderr, "Transmission failed\n");
            gpio_end_tx();
            break;
        }

        // Return to RX mode
        gpio_end_tx();

        // Wait for next transmission
        printf("Waiting %d seconds for next TX...\n", config.tx_interval_sec);
        for (int i = 0; i < config.tx_interval_sec && running; i++) {
            sleep(1);
        }
    }

    // Cleanup
    printf("\nShutting down...\n");
    pluto_cleanup(pluto);
    gpio_cleanup();

    printf("Shutdown complete\n");
    return 0;
}
