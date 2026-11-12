#ifndef APP_H
#define APP_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

void app_init(I2C_HandleTypeDef *hi2c, UART_HandleTypeDef *huart);
void app_tick(uint32_t now);

#endif
