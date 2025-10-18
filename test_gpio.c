#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

// GPIO pins for Odroid-C2 (from gpio_control.h)
#define GPIO_PA_ENABLE  605   // J2 Pin 35
#define GPIO_RELAY_TX   609   // J2 Pin 36
#define GPIO_LED_TX     610   // J2 Pin 31
#define GPIO_LED_STATUS 615   // J2 Pin 32

// Export GPIO
int gpio_export(int gpio_num) {
    char buffer[64];
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        perror("Failed to open /sys/class/gpio/export");
        return -1;
    }

    int len = snprintf(buffer, sizeof(buffer), "%d", gpio_num);
    if (write(fd, buffer, len) < 0) {
        perror("Failed to export GPIO");
        close(fd);
        return -1;
    }

    close(fd);
    usleep(100000); // Wait 100ms for sysfs to create files
    return 0;
}

// Set GPIO direction
int gpio_set_direction(int gpio_num, const char *direction) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio_num);

    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open direction file");
        return -1;
    }

    if (write(fd, direction, strlen(direction)) < 0) {
        perror("Failed to set direction");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

// Set GPIO value
int gpio_set_value(int gpio_num, int value) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio_num);

    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open value file");
        return -1;
    }

    char buf[2] = {value ? '1' : '0', '\0'};
    if (write(fd, buf, 1) < 0) {
        perror("Failed to set value");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

// Unexport GPIO
int gpio_unexport(int gpio_num) {
    char buffer[64];
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd < 0) {
        perror("Failed to open /sys/class/gpio/unexport");
        return -1;
    }

    int len = snprintf(buffer, sizeof(buffer), "%d", gpio_num);
    write(fd, buffer, len);
    close(fd);
    return 0;
}

// Test a single GPIO
int test_gpio(int gpio_num, const char *name) {
    printf("\n=== Testing GPIO %d (%s) ===\n", gpio_num, name);

    // Export
    printf("Exporting GPIO %d...\n", gpio_num);
    if (gpio_export(gpio_num) < 0) {
        printf("Warning: GPIO %d may already be exported\n", gpio_num);
    }

    // Set as output
    printf("Setting GPIO %d as output...\n", gpio_num);
    if (gpio_set_direction(gpio_num, "out") < 0) {
        printf("FAILED to set direction\n");
        gpio_unexport(gpio_num);
        return -1;
    }

    // Blink 3 times
    printf("Blinking GPIO %d 3 times (check with voltmeter/LED)...\n", gpio_num);
    for (int i = 0; i < 3; i++) {
        printf("  HIGH\n");
        gpio_set_value(gpio_num, 1);
        sleep(1);

        printf("  LOW\n");
        gpio_set_value(gpio_num, 0);
        sleep(1);
    }

    // Cleanup
    printf("Unexporting GPIO %d...\n", gpio_num);
    gpio_unexport(gpio_num);

    printf("GPIO %d test complete!\n", gpio_num);
    return 0;
}

int main(int argc, char *argv[]) {
    printf("===========================================\n");
    printf("Odroid-C2 GPIO Test for SARSAT T.001\n");
    printf("===========================================\n");
    printf("\nThis will test the following GPIO pins:\n");
    printf("  GPIO 605 (J2 Pin 35) - PA Enable\n");
    printf("  GPIO 609 (J2 Pin 36) - TX/RX Relay\n");
    printf("  GPIO 610 (J2 Pin 31) - TX LED\n");
    printf("  GPIO 615 (J2 Pin 32) - Status LED\n");
    printf("\nEach pin will toggle HIGH/LOW 3 times.\n");
    printf("Connect a voltmeter or LED to verify.\n");
    printf("\nPress ENTER to start, or Ctrl+C to abort...\n");
    getchar();

    // Test each GPIO
    int gpios[] = {GPIO_PA_ENABLE, GPIO_RELAY_TX, GPIO_LED_TX, GPIO_LED_STATUS};
    const char *names[] = {"PA Enable", "TX/RX Relay", "TX LED", "Status LED"};

    for (int i = 0; i < 4; i++) {
        if (test_gpio(gpios[i], names[i]) < 0) {
            printf("\n*** GPIO %d FAILED ***\n", gpios[i]);
        }

        if (i < 3) {
            printf("\nPress ENTER for next GPIO...\n");
            getchar();
        }
    }

    printf("\n===========================================\n");
    printf("All GPIO tests complete!\n");
    printf("===========================================\n");

    return 0;
}
