#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H

#include <stdbool.h>

// =============================
// GPIO Control for Odroid C2/M1S
// =============================

// GPIO pin assignments for Odroid C2 (J2 Header)
// Based on /sys/kernel/debug/gpio mapping
// All pins use /sys/class/gpio/gpioXXX interface

#define GPIO_PA_ENABLE  605   // J2 Pin 35 - PA 5W enable (active high)
#define GPIO_RELAY_TX   609   // J2 Pin 36 - TX/RX relay (high=TX, low=RX)
#define GPIO_LED_TX     610   // J2 Pin 31 - TX indicator LED (active high)
#define GPIO_LED_STATUS 615   // J2 Pin 32 - Error/Status LED (active high)

// GPIO states
typedef enum {
    GPIO_LOW = 0,
    GPIO_HIGH = 1
} gpio_state_t;

// =============================
// Initialization & Cleanup
// =============================

// Initialize GPIO (export pins, set directions)
bool gpio_init(void);

// Cleanup GPIO (unexport pins)
void gpio_cleanup(void);

// =============================
// GPIO Control Functions
// =============================

// Export GPIO pin
bool gpio_export(int gpio_num);

// Unexport GPIO pin
bool gpio_unexport(int gpio_num);

// Set GPIO direction (in/out)
bool gpio_set_direction(int gpio_num, const char *direction);

// Set GPIO value (0 or 1)
bool gpio_set_value(int gpio_num, gpio_state_t value);

// Read GPIO value
int gpio_get_value(int gpio_num);

// =============================
// RF Control Functions
// =============================

// Enable PA (power amplifier)
bool gpio_pa_enable(bool enable);

// Set TX/RX relay (true = TX mode, false = RX mode)
bool gpio_set_tx_mode(bool tx_mode);

// Control status LED
bool gpio_status_led(bool on);

// Control TX indicator LED
bool gpio_tx_led(bool on);

// =============================
// Convenience Functions
// =============================

// Prepare for transmission (PA on, relay to TX, TX LED on)
bool gpio_prepare_tx(void);

// Return to RX mode (PA off, relay to RX, TX LED off)
bool gpio_end_tx(void);

#endif // GPIO_CONTROL_H
