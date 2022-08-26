/*
    Motor Fonksiyonları
    -motor_move_position
    -Interrupt : Switche değdiğinde duracak 
*/

#include <stdio.h>
#include<stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "sdkconfig.h"

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
TaskHandle_t ISR = NULL; //Interrupt handle
//Global variables.
int curr_pos = 0;
int pos =0 ;
// Map function for conversion (0-9600) -> (0-255). Used this for uart data
int map(int x, int in_min, int in_max, int out_min, int out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


void motor_move_position(int motor_pos)
{
    motor_pos=pos;
    vTaskDelay(8000/portTICK_PERIOD_MS);
    printf("Motor_move_position started .. \n");
    
    printf("Works fine .. \n");
    
    if(curr_pos < motor_pos)
    {
        gpio_set_level(DIR_PIN , 1);
        for(int32_t i = curr_pos ; i<= motor_pos; i++)
        {
            gpio_set_level(EN_PIN, 0);
            gpio_set_level(STEP_PIN , 0);
            ets_delay_us(30);
            gpio_set_level(STEP_PIN , 1);
            ets_delay_us(30);
            curr_pos =   i ;
            printf("current position : %d \n",curr_pos);
            if(curr_pos ==  motor_pos)
            {
                gpio_set_level(EN_PIN, 1);           
                vTaskSuspend(motor_move_handle);
            }
            else
            {
                vTaskResume(motor_move_handle);
            }
        }
    }
    else if (curr_pos>motor_pos)
    {
        gpio_set_level(DIR_PIN , 0);
        for(int32_t i = curr_pos ; i>= motor_pos; i--)
        {
            gpio_set_level(EN_PIN, 0);
            gpio_set_level(STEP_PIN , 0);
            ets_delay_us(30);
            gpio_set_level(STEP_PIN , 1);
            ets_delay_us(30);
            curr_pos =   i ;
            printf("current position : %d \n",curr_pos);
            if(curr_pos ==  motor_pos)
            {
                gpio_set_level(EN_PIN, 1);           
                vTaskSuspend(motor_move_handle);
            }
            else
            {
                vTaskResume(motor_move_handle);
            }
        }
    }
    else
    {
        vTaskSuspend(motor_move_handle);
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
    
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    int *data = (int *) malloc(RX_BUF_SIZE);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_2, data, RX_BUF_SIZE, 4000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            //data[rxBytes] = 0;
            //ESP_LOGI(RX_TASK_TAG, "Read %d bytes: %d", rxBytes,data);
            //ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
            printf("Read %d byte data data is : %d \n", rxBytes, *data);
            int receiving_pos =map(*data,0,255,0,9600);
            pos = receiving_pos;
            printf("Motor should move : %d \n",receiving_pos);
        
        }
    }
    free(data);
}


void IRAM_ATTR button_isr_handler()
{
    xTaskResumeFromISR(ISR);
}


void button_task(void)
{
    bool buton_status =true;
    while (1)
    {
        
        vTaskSuspend(NULL);
        buton_status = !buton_status;
        gpio_set_level(EN_PIN, buton_status);
        printf("Buton pressed \n");
        if(buton_status == true)
        {
            vTaskSuspend(motor_move_handle);
        }
        else
        {
            vTaskResume(motor_move_handle);
        }
      
        
        
    }

}


void app_main() 
{
    gpio_pad_select_gpio(CONFIG_BUTTON_PIN);
    gpio_pad_select_gpio(STEP_PIN);
    gpio_pad_select_gpio(DIR_PIN);
    gpio_pad_select_gpio(EN_PIN);

    gpio_set_direction(CONFIG_BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(STEP_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIR_PIN, GPIO_MODE_OUTPUT);

    gpio_set_intr_type (CONFIG_BUTTON_PIN, GPIO_INTR_NEGEDGE);
   
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    gpio_isr_handler_add(CONFIG_BUTTON_PIN, button_isr_handler, NULL);

    uart_init();
    //xTaskCreate(button_task, "button_task",4096, NULL, 10 , &ISR );
    xTaskCreatePinnedToCore(motor_move_position , "motor_move_position", 4096 ,pos, configMAX_PRIORITIES , &motor_move_handle,0);
    xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
 
}




