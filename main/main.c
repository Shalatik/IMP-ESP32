/* 14.12.2022 */
/* Simona Ceskova (xcesko00)*/
/* IMP projekt: PM - ESP32: Pristupovy terminal*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define RED_LED 13
#define GREEN_LED 5

#define ROW1 14
#define ROW2 26
#define ROW3 25
#define ROW4 16
#define COL1 27
#define COL2 12
#define COL3 17

#define OUTPUT_PIN_MASK ((1ULL << COL1) | (1ULL << COL2) | (1ULL << COL3))
#define INPUT_PIN_MASK ((1ULL << ROW1) | (1ULL << ROW2) | (1ULL << ROW3) | (1ULL << ROW4))
#define ESP_INTR_FLAG_DEFAULT 0

static QueueHandle_t event_queue = NULL;
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(event_queue, &gpio_num, NULL);
}
// global led values for code redusing
int R_LED_state = 0;
int G_LED_state = 0;

char code_array[5] = {'1', '2', '3', '4'};
char test_array[11]; // max lenght = # 1234*1234 = 10

// number of pushed buttons
int counter = 0;

// for using same while loop for setting a new password and also for logging in
// tells when to use password storing function
int new_password_mode = 0;

/* ********************** HELP FUNCTIONS ******************** */

// array with temp values of pushed buttons
// at the beginning and after each log in try
void init_test_array(void)
{
    for (int i = 0; i < sizeof(test_array) / sizeof(test_array[0]); i++)
        test_array[i] = 'x';
}

// counting character for one password
void print_output(char ch)
{
    printf("%c .... counter:", ch);
}

// makes a led to blink, gpio is set to level (0 or 1) = R_LED_state
void led_flash(void)
{
    gpio_set_level(RED_LED, R_LED_state);
    gpio_set_level(GREEN_LED, G_LED_state);
}

// led blinking mode when storing a new password
void both_LED(void)
{
    R_LED_state = !R_LED_state;
    G_LED_state = !G_LED_state;
    led_flash();
}

// mode for situations: password for log in is correct, a new password is sucesfully set
void good_G_LED(void)
{
    printf("Password is correct!\n");
    G_LED_state = 1;
    R_LED_state = 0;
    led_flash();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    G_LED_state = 0;
    R_LED_state = 1;
    led_flash();
}

// mode is used when wrong password is typed for log in and checking old password before setting a new one
void wrong_R_LED(void)
{
    G_LED_state = 0;
    R_LED_state = 0;
    for (int i = 0; i < 5; i++)
    {
        R_LED_state = !R_LED_state;
        led_flash();
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    R_LED_state = 1;
    led_flash();
}
/* ********************** HELP FUNCTIONS ******************** */

// checking if typed password is correct or not
void compare()
{
    counter = 0;
    for (int i = 0; i < 4; i++)
    {
        if (code_array[i] != test_array[i])
        {
            printf("Wrong password!\n");
            wrong_R_LED();
            return;
        }
    }
    good_G_LED();
}

// this is check before changing a password if the old one is correct
void before_changing_password()
{
    for (int i = 1; i < 5; i++)
    {
        if (code_array[i - 1] != test_array[i])
        {
            printf("wrong special\n");
            wrong_R_LED();
            return;
        }
    }
    // old password is correct, opens mode for storing a new password
    new_password_mode = 1;
}

// storing new password while both LED are blinking in while loop
void store_new_password(void)
{
    code_array[counter - 1] = test_array[counter - 1];
    if (counter == 4)
    {
        good_G_LED();
        // end of storing
        new_password_mode = 0;
        init_test_array();
        counter = 0;
        printf("Password saved successfully!\n");
        return;
    }
}

// adding one character of password to temp array called test_array
void add_char(char ch)
{
    print_output(ch);
    test_array[counter] = ch;
    counter++;
    printf("%i\n", counter);
    if (new_password_mode)
    {
        store_new_password();
    }
    else if (test_array[0] != '#') // controling that it is not special code sequence
    {
        if (counter == 4)
        {
            compare();
            init_test_array();
        }
    }
    else if (counter == 6)
    {
        if (test_array[5] == '*') // end of special sequence
        {
            // controle if old password is correct
            before_changing_password();
            // either way counter and array has to be initialized
            // if old password is correct, a new one is going to be saved in temp test_array
            counter = 0;
            init_test_array();
        }
        else // FIXME:
        {
            wrong_R_LED();
            counter = 0;
            init_test_array();
        }
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
}

// before some process initialization must to be done
void init_ports(void)
{
    gpio_reset_pin(RED_LED);
    gpio_reset_pin(GREEN_LED);
    gpio_reset_pin(ROW1);
    gpio_reset_pin(ROW2);
    gpio_reset_pin(ROW3);
    gpio_reset_pin(ROW4);
    gpio_reset_pin(COL1);
    gpio_reset_pin(COL2);
    gpio_reset_pin(COL3);

    gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(GREEN_LED, GPIO_MODE_OUTPUT);

    gpio_config_t gpio_cfg = {
        .intr_type = GPIO_INTR_DISABLE, // interrupt of rising edge
        .pin_bit_mask = OUTPUT_PIN_MASK,    // bit mask for  GPIOs 4 and 5
        .mode = GPIO_MODE_OUTPUT,           // set GPIOs 4 and 5 as inputs
        .pull_up_en = 0,                    // enable pull-up mode
        .pull_down_en = 0,                  // enable pull-up mode
    };

    gpio_config(&gpio_cfg); // set the configuration

    // prepare configuration for the input pins 4 and 5
    gpio_cfg.pin_bit_mask = INPUT_PIN_MASK;    // set as output mode...
    gpio_cfg.mode = GPIO_MODE_INPUT;       // ...using an appropriate bit mask, here for GPIO 18 and 19
    gpio_cfg.pull_up_en = 0;               // disable pull-down mode
    gpio_cfg.pull_down_en = 1;             // disable pull-up mode
    gpio_cfg.intr_type = GPIO_INTR_POSEDGE; // rising edge

    gpio_config(&gpio_cfg);
}

// disables all interrupts while we do not need them
void disable_interrupt()
{
    gpio_set_level(COL2, 0);
    gpio_set_level(COL1, 0);
    gpio_set_level(COL3, 0);
    gpio_intr_disable(COL1);
    gpio_intr_disable(COL2);
    gpio_intr_disable(COL3);
}

// after interrupt process, they can be again enabled
void enable_interrupt()
{
    gpio_set_level(COL2, 1);
    gpio_set_level(COL1, 1);
    gpio_set_level(COL3, 1);
    gpio_intr_enable(COL1);
    gpio_intr_enable(COL2);
    gpio_intr_enable(COL3);
}

void while_loop(void *arg)
{
    uint32_t io_num;
    while (1)
    {
        if (xQueueReceive(event_queue, &io_num, portMAX_DELAY))
        {
            disable_interrupt();

            gpio_set_level(COL1, 1);
            if (gpio_get_level(ROW1) == 1)
                add_char('1');
            else if (gpio_get_level(ROW2) == 1)
                add_char('4');
            else if (gpio_get_level(ROW3) == 1)
                add_char('7');
            else if (gpio_get_level(ROW4) == 1)
                add_char('*');
            gpio_set_level(COL1, 0);

            gpio_set_level(COL2, 1);
            if (gpio_get_level(ROW1) == 1)
                add_char('2');
            if (gpio_get_level(ROW2) == 1)
                add_char('5');
            if (gpio_get_level(ROW3) == 1)
                add_char('8');
            if (gpio_get_level(ROW4) == 1)
                add_char('0');
            gpio_set_level(COL2, 0);

            gpio_set_level(COL3, 1);
            if (gpio_get_level(ROW1) == 1)
                add_char('3');
            if (gpio_get_level(ROW2) == 1)
                add_char('6');
            if (gpio_get_level(ROW3) == 1)
                add_char('9');
            if (gpio_get_level(ROW4) == 1)
                add_char('#');
            gpio_set_level(COL3, 0);

            if (new_password_mode)
                both_LED();
            vTaskDelay(200 / portTICK_PERIOD_MS);

            enable_interrupt();
        }
    }
}

void init_gpio()
{
    enable_interrupt();

    event_queue = xQueueCreate(10, sizeof(uint32_t));            // create a queue to handle gpio event from isr
    xTaskCreate(while_loop, "while_loop", 2048, NULL, 10, NULL); // start gpio task

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);            // install gpio interrupt service routine...
    gpio_isr_handler_add(ROW1, gpio_isr_handler, (void *)ROW1); //...to be called from IN1 (GPIO4)
    gpio_isr_handler_add(ROW2, gpio_isr_handler, (void *)ROW2); //...to be called from IN1 (GPIO4)
    gpio_isr_handler_add(ROW3, gpio_isr_handler, (void *)ROW3); //...to be called from IN1 (GPIO4)
    gpio_isr_handler_add(ROW4, gpio_isr_handler, (void *)ROW4); //...to be called from IN1 (GPIO4)
}

void app_main(void)
{
    // preparation
    init_test_array();
    init_ports();

    // preparatin and creating a new process
    init_gpio();

    // basic program state
    R_LED_state = !R_LED_state;
    led_flash();
}