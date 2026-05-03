#ifndef VEML7700_H
#define VEML7700_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VEML7700_I2C_ADDR        (0x10U << 1)

#define VEML7700_REG_ALS_CONF    0x00U
#define VEML7700_REG_ALS_WH      0x01U
#define VEML7700_REG_ALS_WL      0x02U
#define VEML7700_REG_POWER_SAVE  0x03U
#define VEML7700_REG_ALS         0x04U
#define VEML7700_REG_WHITE       0x05U
#define VEML7700_REG_INTERRUPT   0x06U

typedef enum {
    VEML7700_GAIN_1   = 0x00U,
    VEML7700_GAIN_2   = 0x01U,
    VEML7700_GAIN_1_8 = 0x02U,
    VEML7700_GAIN_1_4 = 0x03U
} VEML7700_Gain;

typedef enum {
    VEML7700_IT_100MS = 0x00U,
    VEML7700_IT_200MS = 0x01U,
    VEML7700_IT_400MS = 0x02U,
    VEML7700_IT_800MS = 0x03U,
    VEML7700_IT_50MS  = 0x08U,
    VEML7700_IT_25MS  = 0x0CU
} VEML7700_IntegrationTime;

typedef struct {
    I2C_HandleTypeDef *hi2c;
    VEML7700_Gain gain;
    VEML7700_IntegrationTime integration_time;
    uint32_t last_read_ms;
} VEML7700_HandleTypeDef;

HAL_StatusTypeDef VEML7700_Init(VEML7700_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef VEML7700_SetGain(VEML7700_HandleTypeDef *dev, VEML7700_Gain gain);
HAL_StatusTypeDef VEML7700_SetIntegrationTime(VEML7700_HandleTypeDef *dev, VEML7700_IntegrationTime it);
HAL_StatusTypeDef VEML7700_ReadALSRaw(VEML7700_HandleTypeDef *dev, uint16_t *raw);
HAL_StatusTypeDef VEML7700_ReadWhiteRaw(VEML7700_HandleTypeDef *dev, uint16_t *raw);
HAL_StatusTypeDef VEML7700_ReadLuxX10(VEML7700_HandleTypeDef *dev, uint32_t *lux_x10);
HAL_StatusTypeDef VEML7700_ReadLuxAutoX10(VEML7700_HandleTypeDef *dev, uint32_t *lux_x10);

#ifdef __cplusplus
}
#endif

#endif
