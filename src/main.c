#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
//Stepper motor definitions
#define CONFIG_BUTTON_PIN 0 //Motoru durdurmak için esp32 boot tuşunun pin numarası
#define ESP_INTR_FLAG_DEFAULT 0
#define STEP_PIN 19
#define DIR_PIN 18
#define EN_PIN 21
#define MAX_POS 9600
//UART definitions
#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)
#define UART UART_NUM_2
static const int RX_BUF_SIZE = 1024;

TaskHandle_t motor_move_handle = NULL; //motor move handle


int dummy_pose = 0;
int *pos ;
int *curr_pos ;
bool start_move = false;
bool read_data = true;

// Map function for conversion (0-9600) -> (0-255). Used this for uart data
int map(int x, int in_min, int in_max, int out_min, int out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


void motor_move_position()
{
    vTaskDelay(4000/portTICK_RATE_MS);
    int current_pos =0;
    curr_pos=&current_pos;
    //pos=&dummy_pose;
    //*pos = 8000;
    vTaskDelay(2500/portTICK_RATE_MS);
    printf("Motor should move  position: %d \n ",*pos);
    printf("Motor current position : %d \n",*curr_pos);
   
   
    while(1)
    {
    if(start_move)
    {

    if(*curr_pos < *pos)
        {
        read_data = false;
        gpio_set_level(DIR_PIN , 1);
            for(int32_t i = *curr_pos ; i<= *pos; i++)
            {
                gpio_set_level(EN_PIN, 0);
                gpio_set_level(STEP_PIN , 0);
                ets_delay_us(10);
                gpio_set_level(STEP_PIN , 1);
                ets_delay_us(10);
                *curr_pos =   i ;
                printf("current position : %d \n",*curr_pos);
                if(*curr_pos ==  *pos)
                {
                    gpio_set_level(EN_PIN, 1);           
                    vTaskSuspend(motor_move_handle);
                    read_data = true;
                }
                else
                {
                    vTaskResume(motor_move_handle);
                    read_data=false;
                }
            }
        }

        else if (*curr_pos>*pos)
        {
        read_data = false;
        gpio_set_level(DIR_PIN , 0);
            for(int32_t i = *curr_pos ; i>= *pos; i--)
            {
                gpio_set_level(EN_PIN, 0);
                gpio_set_level(STEP_PIN , 0);
                ets_delay_us(10);
                gpio_set_level(STEP_PIN , 1);
                ets_delay_us(10);
                *curr_pos =   i ;
                printf("current position : %d \n",*curr_pos);
                if(*curr_pos ==  *pos)
                {
                    gpio_set_level(EN_PIN, 1);           
                    vTaskSuspend(motor_move_handle);
                    read_data=true;

                }
                else
                {
                    vTaskResume(motor_move_handle);
                    read_data=false;
                }
            }
        }
        else
        {
            vTaskSuspend(motor_move_handle);
            read_data=true;
       }
    }
    }
} 

void uart_init(void) 
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // We won't use a buffer for sending data.
    uart_driver_install(UART, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART, &uart_config);
    uart_set_pin(UART, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}
static void rx_task(void *arg)
{
    

    int *data = (int *) malloc(RX_BUF_SIZE);
    if(read_data)
    {
        while (1) {
            const int rxBytes = uart_read_bytes(UART_NUM_2, data, RX_BUF_SIZE, 4000 / portTICK_PERIOD_MS);
            if (rxBytes > 0) {
                //data[rxBytes] = 0;
                //ESP_LOGI(RX_TASK_TAG, "Read %d bytes: %d", rxBytes,data);
                //ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
                printf("Read %d byte data data is : %d \n", rxBytes, *data);
                int receiving_pos =map(*data,0,255,0,9600);
                pos = &receiving_pos;
                printf("Motor should move : %d \n",*pos);
                if(*pos!=curr_pos)
                {
                    vTaskDelay(3000/portTICK_RATE_MS);
                    start_move = true;
                    vTaskResume(motor_move_handle);
                }
                else
                {
                    vTaskSuspend(motor_move_handle);
                }
            
        
        }
    }
    free(data);
    }
}


void app_main() 
{

    gpio_pad_select_gpio(STEP_PIN);
    gpio_pad_select_gpio(DIR_PIN);
    gpio_pad_select_gpio(EN_PIN);

    gpio_set_direction(STEP_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIR_PIN, GPIO_MODE_OUTPUT);
    uart_init();

    xTaskCreate(motor_move_position, "motor_move_position", 4096, NULL,configMAX_PRIORITIES, &motor_move_handle );
    xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);

}