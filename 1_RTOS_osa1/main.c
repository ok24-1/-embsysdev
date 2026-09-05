#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>


// Tehtävä on toteuttettu tällä hetkellä kahden pisteen arvoisesti. Täydennetään kolmen pisteen arvoiseksi myöhemmin.


// Configure buttons
#define BUTTON_PAUSE DT_ALIAS(sw0)
#define DEBOUNCE_MS 200
// #define BUTTON_1 DT_ALIAS(sw1)
static const struct gpio_dt_spec button_pause = GPIO_DT_SPEC_GET_OR(BUTTON_PAUSE, gpios, {0});
static struct gpio_callback button_pause_data;
static int64_t last_button_time = 0;


// Led pin configurations
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
//static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);


void red_led_task(void *, void *, void*);
void green_led_task(void *, void *, void*);
void yellow_led_task(void *, void *, void*);
int init_led(void);
int init_button(void);

#define STACKSIZE 500
#define PRIORITY 5

K_THREAD_DEFINE(red_thread,STACKSIZE,red_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(yellow_thread,STACKSIZE,yellow_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(green_thread,STACKSIZE,green_led_task,NULL,NULL,NULL,PRIORITY,0,0);

volatile int led_state = 0;
volatile int last_state = 0;

// Button interrupt handler
void button_pause_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
        
        int64_t now = k_uptime_get();
        if ((now - last_button_time) < DEBOUNCE_MS) {
        return;
        }
        last_button_time = now;

	printk("Pause button pressed:\n");

        if (led_state != 4){
                last_state = led_state;
                led_state = 4;
        }
        else {
                led_state = last_state;
        }
  

}

// Main program
int main(void)
{
	init_led();
	init_button();

	return 0;
}

int init_button() {

	int ret;
	if (!gpio_is_ready_dt(&button_pause)) {
		printk("Error: button pause is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_pause, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_pause, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_pause_data, button_pause_handler, BIT(button_pause.pin));
	gpio_add_callback(button_pause.port, &button_pause_data);
	printk("Set up button pause ok\n");
	
	return 0;
}

// Initialize leds
int  init_led(void) {

	// Led pin initialization
        int ret = 0;
	ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Led configure failed\n");		
		return ret;
	}
        gpio_pin_set_dt(&red,0);
	ret = gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Led configure failed\n");		
		return ret;
	}
	 gpio_pin_set_dt(&green,0);	

	printk("Led initialized ok\n");
	
	return 0;
}

// Task to handle red led
void red_led_task(void *, void *, void*) {

	printk("Red led thread started\n");

	while (true) {
                if (led_state == 0) {
                        gpio_pin_set_dt(&red,1);
                        k_sleep(K_SECONDS(1));
                        if (led_state == 4) {
                                k_sleep(K_SECONDS(1));
                                continue;
                        }
                        gpio_pin_set_dt(&red,0);
                        led_state = 1;
	                }
                k_msleep(100);
        }       
}


void yellow_led_task(void *, void *, void*) {
	
	printk("Yellow led thread started\n");

	while (true) {
                if (led_state == 1) {
                        gpio_pin_set_dt(&green,1);
                        gpio_pin_set_dt(&red,1);
                        k_sleep(K_SECONDS(1));
                        if (led_state == 4) {
                                k_sleep(K_SECONDS(1));
                                continue;
                        }
                        gpio_pin_set_dt(&green,0);
                        gpio_pin_set_dt(&red,0);
                        led_state = 2;
	        }
                k_msleep(100);
        }
}


void green_led_task(void *, void *, void*) {

	printk("Green led thread started\n");
	while (true) {
                if (led_state == 2) {
                        gpio_pin_set_dt(&green,1);
                        k_sleep(K_SECONDS(1));
                        if (led_state == 4) {
                                k_sleep(K_SECONDS(1));
                                continue;
                        }
                        gpio_pin_set_dt(&green,0);
                        led_state = 0;
	                }
                k_msleep(100);
        }
}
