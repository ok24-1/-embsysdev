#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/timing/timing.h>
#include "TimeParser.h"


// Tehtävään toteutettu Yksinkertaiset testit ja parseri sulautetussa ohjelmassa sekä Lisätään testikeissejä

// Thread initializations
#define STACKSIZE 500
#define PRIORITY 5

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
volatile int led_state = 0;
volatile int last_state = 0;

// UART initialization
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

// Condition Variables
K_SEM_DEFINE(red_signal, 0, 1);
K_MUTEX_DEFINE(yellow_mutex);
K_CONDVAR_DEFINE(yellow_signal);
K_MUTEX_DEFINE(green_mutex);
K_CONDVAR_DEFINE(green_signal);
K_MUTEX_DEFINE(release_mutex);
K_CONDVAR_DEFINE(release_signal);

static void red_timer_expiry(struct k_timer *timer);
K_TIMER_DEFINE(red_delay_timer, red_timer_expiry, NULL);

// Create dispatcher FIFO buffer
K_FIFO_DEFINE(dispatcher_fifo);
K_FIFO_DEFINE(data_fifo);

void red_led_task(void *, void *, void*);
void green_led_task(void *, void *, void*);
void yellow_led_task(void *, void *, void*);
int init_led(void);
int init_button(void);
static void dispatcher_task(void *, void *, void *);
static void uart_task(void *, void *, void *);
void debug_task(void *, void *, void*);


K_THREAD_DEFINE(dis_thread,STACKSIZE,dispatcher_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(uart_thread,STACKSIZE,uart_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(red_thread,STACKSIZE,red_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(yellow_thread,STACKSIZE,yellow_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(green_thread,STACKSIZE,green_led_task,NULL,NULL,NULL,PRIORITY,0,0);

K_THREAD_DEFINE(debug_thread,STACKSIZE,debug_task,NULL,NULL,NULL,PRIORITY,0,0);

// Debug toggle
volatile bool debug_enabled = false;

static uint64_t total_UART_time = 0;

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

// FIFO dispatcher data type
struct data_t {
	/*************************
	// Add fifo_reserved below
	*************************/
	void *fifo_reserved;
	char msg[20];
        uint64_t task_time;
        uint64_t total_time;
        const char *sender;
};

/********************
 * init UART
 */
int init_uart(void) {
	// UART initialization
	if (!device_is_ready(uart_dev)) {
		return 1;
	} 
	return 0;
}

/********************
 * Main task
 */

#ifndef UNIT_TESTING
int main(void)
{
	int ret = init_uart();
	if (ret != 0) {
		printk("UART initialization failed!\n");
		return ret;
	}

        init_led();
	init_button();
        timing_init();

	return 0;
}
#endif

/********************
 * UART task
 */
static void uart_task(void *unused1, void *unused2, void *unused3)
{
	// Received character from UART
	char rc=0;
	// Message from UART
	char uart_msg[20];
	memset(uart_msg,0,20);
	int uart_msg_cnt = 0;

	while (true) {
		// Ask UART if data available
		if (uart_poll_in(uart_dev,&rc) == 0) {
			// printk("Received: %c\n",rc);
			// If character is not newline, add to UART message buffer
			if (rc != '\r') {
				uart_msg[uart_msg_cnt] = rc;
				uart_msg_cnt++;
			// Character is newline, copy dispatcher data and put to FIFO buffer
			} else {
                                if (debug_enabled) {
                                        printk("UART msg: %s\n", uart_msg);
                                }
				struct data_t *buf = k_malloc(sizeof(struct data_t));
				if (buf == NULL) {
					return;
				}
				// Copy UART message to dispatcher data
				// strncpy(buf->msg, 20, uart_msg); // mitä ihmettä, miksi kaatuu!!
				 snprintf(buf->msg, 20, "%s", uart_msg);

				// You need to:
				// Put dispatcher data to FIFO buffer
                                k_fifo_put(&dispatcher_fifo, buf);

				// Clear UART receive buffer
				uart_msg_cnt = 0;
				memset(uart_msg,0,20);

				// Clear UART message buffer
				uart_msg_cnt = 0;
				memset(uart_msg,0,20);
			}
		}
		k_msleep(10);
	}
	//return 0;
}

/********************
 * Dispatcher task
 */
static void dispatcher_task(void *unused1, void *unused2, void *unused3)
{
	while (true) {
		// Receive dispatcher data from uart_task fifo
		struct data_t *rec_item = k_fifo_get(&dispatcher_fifo, K_FOREVER);
		char sequence[20];
		memcpy(sequence,rec_item->msg,20);
		k_free(rec_item);
                
                if (debug_enabled) {
                        printk("Dispatcher received: %s\n", sequence);
                }

				bool is_color_sequence = sequence[0] != '\0';
				for (size_t i = 0; sequence[i] != '\0'; ++i) {
					if (sequence[i] != 'D' && sequence[i] != 'R' &&
					    sequence[i] != 'G' && sequence[i] != 'Y') {
						is_color_sequence = false;
						break;
					}
				}

				if (!is_color_sequence && sequence[0] != '\0') {
					int delay_seconds = time_parse(sequence);
					if (delay_seconds < 0) {
						printk("Invalid time (%d)\n", delay_seconds);
						continue;
					}

					k_timer_start(&red_delay_timer, K_SECONDS(delay_seconds), K_NO_WAIT);
					printk("Red LED scheduled in %d seconds\n", delay_seconds);
					continue;
				}
                
                total_UART_time = 0;
                int cnt = 0;

                while (sequence[cnt] != 0) {

                         if (sequence[cnt] == 'D'){
                                debug_enabled = !debug_enabled;
                                printk("Debug mode: %s\n", debug_enabled ? "ON" : "OFF");
                                cnt++;
                                continue;
						}
                        if (sequence[cnt] == 'R'){
                                if (debug_enabled) {
                                        printk("Red ");
                                }
							k_sem_give(&red_signal);
                        }
                        if (sequence[cnt] == 'G'){
                                if (debug_enabled) {
                                        printk("Green ");
                                }
                                k_condvar_broadcast(&green_signal);
                        }
                        if (sequence[cnt] == 'Y'){
                                if (debug_enabled) {
                                        printk("Yellow ");
                                }
                                k_condvar_broadcast(&yellow_signal);
                        }
                        cnt++;
                        k_condvar_wait(&release_signal, &release_mutex, K_FOREVER);
               // }
        // You need to:
        // Parse color and time from the fifo data
        // Example
           // char color = sequence[0];
            //int time = atoi(sequence+2);
		    //printk("Data: %c %d\n", color, time);
        // Send the parsed color information to tasks using fifo
        // Use release signal to control sequence or k_yield
	}
}
}

static void red_timer_expiry(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	k_sem_give(&red_signal);
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


	while (true) {
				k_sem_take(&red_signal, K_FOREVER);

                timing_start();
                timing_t start_time = timing_counter_get();
                
                if (debug_enabled) {
                        printk("Red led thread started\n");
                }
                
                gpio_pin_set_dt(&red,1);
                k_sleep(K_SECONDS(1));
                gpio_pin_set_dt(&red,0);
                k_msleep(100);

                struct data_t *buf = k_malloc(sizeof(struct data_t));
		        if (buf == NULL) {
			return;
		}

		timing_stop();
		timing_t end_time = timing_counter_get();
                uint64_t diff = timing_cycles_to_ns(timing_cycles_get(&start_time, &end_time));

                total_UART_time += diff;
                buf->total_time = total_UART_time;
                buf->task_time = diff;
                buf->sender = "Red";
		k_fifo_put(&data_fifo, buf);

                k_condvar_broadcast(&release_signal);
        }       
}


void yellow_led_task(void *, void *, void*) {
        
	while (true) {
                k_condvar_wait(&yellow_signal, &yellow_mutex, K_FOREVER);

                timing_start();
                timing_t start_time = timing_counter_get();
                
                if (debug_enabled) {
                        printk("Yellow led thread started\n");
                }
                
                gpio_pin_set_dt(&red,1);
                gpio_pin_set_dt(&green,1);
                k_sleep(K_SECONDS(1));
                gpio_pin_set_dt(&red,0);
                gpio_pin_set_dt(&green,0);
                k_msleep(100);

                struct data_t *buf = k_malloc(sizeof(struct data_t));
		        if (buf == NULL) {
			return;
		}

		timing_stop();
		timing_t end_time = timing_counter_get();
                uint64_t diff = timing_cycles_to_ns(timing_cycles_get(&start_time, &end_time));

                total_UART_time += diff;
                buf->task_time = diff;
                buf->total_time = total_UART_time;
                buf->sender = "Yellow";
		k_fifo_put(&data_fifo, buf);

                k_condvar_broadcast(&release_signal);
        }  
}


void green_led_task(void *, void *, void*) {
 
	while (true) {
                k_condvar_wait(&green_signal, &green_mutex, K_FOREVER);

                timing_start();
                timing_t start_time = timing_counter_get();
                
                if (debug_enabled) {
                        printk("Green led thread started\n");
                }
                
                gpio_pin_set_dt(&green,1);
                k_sleep(K_SECONDS(1));
                gpio_pin_set_dt(&green,0);
                k_msleep(100);

                struct data_t *buf = k_malloc(sizeof(struct data_t));
		        if (buf == NULL) {
			return;
		}

		timing_stop();
		timing_t end_time = timing_counter_get();
                uint64_t diff = timing_cycles_to_ns(timing_cycles_get(&start_time, &end_time));

                buf->task_time = diff;
		total_UART_time += diff;
                buf->total_time = total_UART_time;
                buf->sender = "Green";
		k_fifo_put(&data_fifo, buf);

                k_condvar_broadcast(&release_signal);
        }      
}



void debug_task(void *, void *, void*) {

	// Store received data
	struct data_t *received;

	while (true) {

		received = k_fifo_get(&data_fifo, K_FOREVER);
                if (debug_enabled) {
		printk("Debug received from %s: %" PRIu64 " | Total time: %" PRIu64 "\n", received->sender, received->task_time, received->total_time);
                }
		k_free(received);

		k_yield();
	}
}




