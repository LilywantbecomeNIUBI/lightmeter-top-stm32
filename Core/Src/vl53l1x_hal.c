#include "vl53l1x_hal.h"
#include <string.h>

#define VL53L1X_I2C_ADDR_8BIT        (VL53L1X_I2C_ADDR_7BIT << 1)
#define VL53L1X_I2C_TIMEOUT_MS       20U
#define VL53L1X_BOOT_TIMEOUT_MS      120U

#define REG_GPIO_HV_MUX_CTRL         0x0030U
#define REG_SYSTEM_INTERRUPT_CLEAR   0x0086U
#define REG_SYSTEM_MODE_START        0x0087U
#define REG_RESULT_RANGE_STATUS      0x0089U
#define REG_RESULT_DISTANCE_MM       0x0096U
#define REG_FIRMWARE_SYSTEM_STATUS   0x00E5U
#define REG_IDENTIFICATION_MODEL_ID  0x010FU

/* ST/SparkFun VL53L1X default configuration table for registers 0x002D..0x0087.
 * The final byte at 0x0087 is kept 0x00 so ranging is started explicitly later.
 */
static const uint8_t kDefaultConfig[] = {
  0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x02, 0x08,
  0x00, 0x08, 0x10, 0x01, 0x01, 0x00, 0x00, 0x00,
  0x00, 0xff, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x20, 0x0b, 0x00, 0x00, 0x02, 0x0a, 0x21,
  0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0xc8,
  0x00, 0x00, 0x38, 0xff, 0x01, 0x00, 0x08, 0x00,
  0x00, 0x01, 0xdb, 0x0f, 0x01, 0xf1, 0x0d, 0x01,
  0x68, 0x00, 0x80, 0x08, 0xb8, 0x00, 0x00, 0x00,
  0x00, 0x0f, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x01, 0x0f, 0x0d, 0x0e, 0x0e, 0x00,
  0x00, 0x02, 0xc7, 0xff, 0x9B, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00
};

static HAL_StatusTypeDef wr8(VL53L1X_HandleTypeDef *h, uint16_t reg, uint8_t data)
{
  return HAL_I2C_Mem_Write(h->hi2c, VL53L1X_I2C_ADDR_8BIT, reg,
                           I2C_MEMADD_SIZE_16BIT, &data, 1U,
                           VL53L1X_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef rd8(VL53L1X_HandleTypeDef *h, uint16_t reg, uint8_t *data)
{
  return HAL_I2C_Mem_Read(h->hi2c, VL53L1X_I2C_ADDR_8BIT, reg,
                          I2C_MEMADD_SIZE_16BIT, data, 1U,
                          VL53L1X_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef rd16(VL53L1X_HandleTypeDef *h, uint16_t reg, uint16_t *data)
{
  uint8_t buf[2] = {0U, 0U};
  HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(h->hi2c, VL53L1X_I2C_ADDR_8BIT, reg,
                                           I2C_MEMADD_SIZE_16BIT, buf, 2U,
                                           VL53L1X_I2C_TIMEOUT_MS);
  if (ret == HAL_OK) {
    *data = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
  }
  return ret;
}

static HAL_StatusTypeDef write_default_config(VL53L1X_HandleTypeDef *h)
{
  return HAL_I2C_Mem_Write(h->hi2c, VL53L1X_I2C_ADDR_8BIT, 0x002DU,
                           I2C_MEMADD_SIZE_16BIT, (uint8_t *)kDefaultConfig,
                           (uint16_t)sizeof(kDefaultConfig),
                           VL53L1X_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef set_interrupt_active_low(VL53L1X_HandleTypeDef *h)
{
  uint8_t v = 0U;
  HAL_StatusTypeDef ret = rd8(h, REG_GPIO_HV_MUX_CTRL, &v);
  if (ret != HAL_OK) return ret;

  /* Bit 4 = 1 means GPIO1/INT active low in this API. This matches PC14 EXTI falling edge. */
  v = (uint8_t)((v & 0xEFU) | 0x10U);
  return wr8(h, REG_GPIO_HV_MUX_CTRL, v);
}

HAL_StatusTypeDef VL53L1X_Attach(VL53L1X_HandleTypeDef *h,
                                 I2C_HandleTypeDef *hi2c,
                                 GPIO_TypeDef *xshut_port,
                                 uint16_t xshut_pin)
{
  if ((h == NULL) || (hi2c == NULL) || (xshut_port == NULL)) {
    return HAL_ERROR;
  }

  memset(h, 0, sizeof(*h));
  h->hi2c = hi2c;
  h->xshut_port = xshut_port;
  h->xshut_pin = xshut_pin;
  return HAL_OK;
}

HAL_StatusTypeDef VL53L1X_ReadID(VL53L1X_HandleTypeDef *h, uint16_t *id)
{
  if ((h == NULL) || (id == NULL)) return HAL_ERROR;
  return rd16(h, REG_IDENTIFICATION_MODEL_ID, id);
}

HAL_StatusTypeDef VL53L1X_PowerOnInit(VL53L1X_HandleTypeDef *h)
{
  uint32_t start_ms;
  uint8_t boot = 0U;
  uint16_t id = 0U;
  HAL_StatusTypeDef ret;

  if ((h == NULL) || (h->hi2c == NULL)) return HAL_ERROR;

  HAL_GPIO_WritePin(h->xshut_port, h->xshut_pin, GPIO_PIN_SET);
  HAL_Delay(2U);

  start_ms = HAL_GetTick();
  do {
    ret = rd8(h, REG_FIRMWARE_SYSTEM_STATUS, &boot);
    if ((ret == HAL_OK) && (boot != 0U)) break;
    HAL_Delay(2U);
  } while ((HAL_GetTick() - start_ms) < VL53L1X_BOOT_TIMEOUT_MS);

  if (boot == 0U) {
    h->initialized = 0U;
    return HAL_TIMEOUT;
  }

  ret = VL53L1X_ReadID(h, &id);
  if ((ret != HAL_OK) || (id != VL53L1X_MODEL_ID_EXPECTED)) {
    h->initialized = 0U;
    return (ret == HAL_OK) ? HAL_ERROR : ret;
  }

  ret = write_default_config(h);
  if (ret != HAL_OK) {
    h->initialized = 0U;
    return ret;
  }

  ret = set_interrupt_active_low(h);
  if (ret != HAL_OK) {
    h->initialized = 0U;
    return ret;
  }

  ret = VL53L1X_ClearInterrupt(h);
  if (ret != HAL_OK) {
    h->initialized = 0U;
    return ret;
  }

  h->initialized = 1U;
  h->ranging = 0U;
  return HAL_OK;
}

void VL53L1X_PowerOff(VL53L1X_HandleTypeDef *h)
{
  if (h == NULL) return;

  if ((h->initialized != 0U) && (h->hi2c != NULL)) {
    (void)VL53L1X_StopRanging(h);
  }

  HAL_GPIO_WritePin(h->xshut_port, h->xshut_pin, GPIO_PIN_RESET);
  h->initialized = 0U;
  h->ranging = 0U;
}

HAL_StatusTypeDef VL53L1X_StartRanging(VL53L1X_HandleTypeDef *h)
{
  HAL_StatusTypeDef ret;
  if ((h == NULL) || (h->initialized == 0U)) return HAL_ERROR;

  ret = VL53L1X_ClearInterrupt(h);
  if (ret != HAL_OK) return ret;

  ret = wr8(h, REG_SYSTEM_MODE_START, 0x40U);
  if (ret == HAL_OK) h->ranging = 1U;
  return ret;
}

HAL_StatusTypeDef VL53L1X_StopRanging(VL53L1X_HandleTypeDef *h)
{
  HAL_StatusTypeDef ret;
  if ((h == NULL) || (h->hi2c == NULL)) return HAL_ERROR;

  ret = wr8(h, REG_SYSTEM_MODE_START, 0x00U);
  if (ret == HAL_OK) {
    h->ranging = 0U;
    (void)VL53L1X_ClearInterrupt(h);
  }
  return ret;
}

HAL_StatusTypeDef VL53L1X_ClearInterrupt(VL53L1X_HandleTypeDef *h)
{
  if ((h == NULL) || (h->hi2c == NULL)) return HAL_ERROR;
  return wr8(h, REG_SYSTEM_INTERRUPT_CLEAR, 0x01U);
}

HAL_StatusTypeDef VL53L1X_ReadAndClear(VL53L1X_HandleTypeDef *h,
                                       uint16_t *distance_mm,
                                       uint8_t *range_status)
{
  HAL_StatusTypeDef ret;
  uint16_t dist = 0U;
  uint8_t status = 0xFFU;

  if ((h == NULL) || (distance_mm == NULL) || (range_status == NULL)) return HAL_ERROR;
  if ((h->initialized == 0U) || (h->ranging == 0U)) return HAL_ERROR;

  ret = rd8(h, REG_RESULT_RANGE_STATUS, &status);
  if (ret != HAL_OK) return ret;

  ret = rd16(h, REG_RESULT_DISTANCE_MM, &dist);
  if (ret != HAL_OK) return ret;

  ret = VL53L1X_ClearInterrupt(h);
  if (ret != HAL_OK) return ret;

  h->last_status = status;
  h->last_distance_mm = dist;
  *range_status = status;
  *distance_mm = dist;
  return HAL_OK;
}
