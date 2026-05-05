#ifndef VL53L1X_HAL_H
#define VL53L1X_HAL_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VL53L1X_I2C_ADDR_7BIT        0x29U
#define VL53L1X_MODEL_ID_EXPECTED    0xEEACU

typedef struct {
  I2C_HandleTypeDef *hi2c;
  GPIO_TypeDef *xshut_port;
  uint16_t xshut_pin;
  uint8_t initialized;
  uint8_t ranging;
  uint8_t last_status;
  uint16_t last_distance_mm;
} VL53L1X_HandleTypeDef;

HAL_StatusTypeDef VL53L1X_Attach(VL53L1X_HandleTypeDef *h,
                                 I2C_HandleTypeDef *hi2c,
                                 GPIO_TypeDef *xshut_port,
                                 uint16_t xshut_pin);
HAL_StatusTypeDef VL53L1X_PowerOnInit(VL53L1X_HandleTypeDef *h);
void VL53L1X_PowerOff(VL53L1X_HandleTypeDef *h);
HAL_StatusTypeDef VL53L1X_StartRanging(VL53L1X_HandleTypeDef *h);
HAL_StatusTypeDef VL53L1X_StopRanging(VL53L1X_HandleTypeDef *h);
HAL_StatusTypeDef VL53L1X_ReadAndClear(VL53L1X_HandleTypeDef *h,
                                       uint16_t *distance_mm,
                                       uint8_t *range_status);
HAL_StatusTypeDef VL53L1X_ClearInterrupt(VL53L1X_HandleTypeDef *h);
HAL_StatusTypeDef VL53L1X_ReadID(VL53L1X_HandleTypeDef *h, uint16_t *id);

#ifdef __cplusplus
}
#endif

#endif /* VL53L1X_HAL_H */
