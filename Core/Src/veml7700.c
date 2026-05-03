#include "veml7700.h"

#define VEML7700_TIMEOUT_MS 100U

static HAL_StatusTypeDef veml_write16(VEML7700_HandleTypeDef *dev, uint8_t reg, uint16_t value)
{
    uint8_t data[2];
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)((value >> 8) & 0xFFU);
    return HAL_I2C_Mem_Write(dev->hi2c, VEML7700_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, data, 2, VEML7700_TIMEOUT_MS);
}

static HAL_StatusTypeDef veml_read16(VEML7700_HandleTypeDef *dev, uint8_t reg, uint16_t *value)
{
    uint8_t data[2];
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->hi2c, VEML7700_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, data, 2, VEML7700_TIMEOUT_MS);
    if (ret == HAL_OK) {
        *value = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    }
    return ret;
}

static uint16_t veml_build_config(VEML7700_Gain gain, VEML7700_IntegrationTime it, uint8_t shutdown)
{
    uint16_t conf = 0;
    conf |= ((uint16_t)gain & 0x03U) << 11;      /* ALS_GAIN bits 12:11 */
    conf |= ((uint16_t)it & 0x0FU) << 6;         /* ALS_IT bits 9:6 */
    conf |= 0U << 4;                             /* persistence = 1 */
    conf |= 0U << 1;                             /* interrupt disabled */
    conf |= shutdown ? 1U : 0U;                  /* ALS_SD */
    return conf;
}

static int veml_it_ms(VEML7700_IntegrationTime it)
{
    switch (it) {
    case VEML7700_IT_25MS:  return 25;
    case VEML7700_IT_50MS:  return 50;
    case VEML7700_IT_100MS: return 100;
    case VEML7700_IT_200MS: return 200;
    case VEML7700_IT_400MS: return 400;
    case VEML7700_IT_800MS: return 800;
    default: return 100;
    }
}

static float veml_gain_value(VEML7700_Gain gain)
{
    switch (gain) {
    case VEML7700_GAIN_1_8: return 0.125f;
    case VEML7700_GAIN_1_4: return 0.25f;
    case VEML7700_GAIN_1:   return 1.0f;
    case VEML7700_GAIN_2:   return 2.0f;
    default: return 1.0f;
    }
}

static float veml_resolution(VEML7700_HandleTypeDef *dev)
{
    /* Same formula as Adafruit library: 0.0036 * (800 / IT) * (2 / gain). */
    return 0.0036f * (800.0f / (float)veml_it_ms(dev->integration_time)) * (2.0f / veml_gain_value(dev->gain));
}

static uint32_t veml_compute_lux_x10(VEML7700_HandleTypeDef *dev, uint16_t raw, uint8_t corrected)
{
    float lux = veml_resolution(dev) * (float)raw;
    if (corrected) {
        lux = (((6.0135e-13f * lux - 9.3924e-9f) * lux + 8.1488e-5f) * lux + 1.0023f) * lux;
    }
    if (lux < 0.0f) lux = 0.0f;
    return (uint32_t)(lux * 10.0f + 0.5f);
}

static void veml_wait_ready(VEML7700_HandleTypeDef *dev)
{
    uint32_t need = (uint32_t)veml_it_ms(dev->integration_time) * 2U;
    uint32_t elapsed = HAL_GetTick() - dev->last_read_ms;
    if (elapsed < need) {
        HAL_Delay(need - elapsed);
    }
}

HAL_StatusTypeDef VEML7700_Init(VEML7700_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret;
    if (dev == 0 || hi2c == 0) return HAL_ERROR;
    dev->hi2c = hi2c;
    dev->gain = VEML7700_GAIN_1_8;
    dev->integration_time = VEML7700_IT_100MS;

    if (HAL_I2C_IsDeviceReady(hi2c, VEML7700_I2C_ADDR, 3, VEML7700_TIMEOUT_MS) != HAL_OK) {
        return HAL_ERROR;
    }

    ret = veml_write16(dev, VEML7700_REG_ALS_CONF, veml_build_config(dev->gain, dev->integration_time, 1));
    if (ret != HAL_OK) return ret;
    ret = veml_write16(dev, VEML7700_REG_POWER_SAVE, 0x0000U);
    if (ret != HAL_OK) return ret;
    ret = veml_write16(dev, VEML7700_REG_ALS_CONF, veml_build_config(dev->gain, dev->integration_time, 0));
    HAL_Delay(5);
    dev->last_read_ms = HAL_GetTick();
    return ret;
}

HAL_StatusTypeDef VEML7700_SetGain(VEML7700_HandleTypeDef *dev, VEML7700_Gain gain)
{
    HAL_StatusTypeDef ret;
    if (dev == 0) return HAL_ERROR;
    dev->gain = gain;
    ret = veml_write16(dev, VEML7700_REG_ALS_CONF, veml_build_config(dev->gain, dev->integration_time, 0));
    dev->last_read_ms = HAL_GetTick();
    return ret;
}

HAL_StatusTypeDef VEML7700_SetIntegrationTime(VEML7700_HandleTypeDef *dev, VEML7700_IntegrationTime it)
{
    HAL_StatusTypeDef ret;
    if (dev == 0) return HAL_ERROR;
    dev->integration_time = it;
    ret = veml_write16(dev, VEML7700_REG_ALS_CONF, veml_build_config(dev->gain, dev->integration_time, 0));
    dev->last_read_ms = HAL_GetTick();
    return ret;
}

HAL_StatusTypeDef VEML7700_ReadALSRaw(VEML7700_HandleTypeDef *dev, uint16_t *raw)
{
    if (dev == 0 || raw == 0) return HAL_ERROR;
    veml_wait_ready(dev);
    dev->last_read_ms = HAL_GetTick();
    return veml_read16(dev, VEML7700_REG_ALS, raw);
}

HAL_StatusTypeDef VEML7700_ReadWhiteRaw(VEML7700_HandleTypeDef *dev, uint16_t *raw)
{
    if (dev == 0 || raw == 0) return HAL_ERROR;
    veml_wait_ready(dev);
    dev->last_read_ms = HAL_GetTick();
    return veml_read16(dev, VEML7700_REG_WHITE, raw);
}

HAL_StatusTypeDef VEML7700_ReadLuxX10(VEML7700_HandleTypeDef *dev, uint32_t *lux_x10)
{
    uint16_t raw;
    HAL_StatusTypeDef ret;
    if (dev == 0 || lux_x10 == 0) return HAL_ERROR;
    ret = VEML7700_ReadALSRaw(dev, &raw);
    if (ret != HAL_OK) return ret;
    *lux_x10 = veml_compute_lux_x10(dev, raw, 1);
    return HAL_OK;
}

HAL_StatusTypeDef VEML7700_ReadLuxAutoX10(VEML7700_HandleTypeDef *dev, uint32_t *lux_x10)
{
    const VEML7700_Gain gains[] = {VEML7700_GAIN_1_8, VEML7700_GAIN_1_4, VEML7700_GAIN_1, VEML7700_GAIN_2};
    const VEML7700_IntegrationTime its[] = {VEML7700_IT_25MS, VEML7700_IT_50MS, VEML7700_IT_100MS, VEML7700_IT_200MS, VEML7700_IT_400MS, VEML7700_IT_800MS};
    uint8_t gi = 0;
    uint8_t ii = 2;
    uint8_t corrected = 0;
    uint16_t raw = 0;
    HAL_StatusTypeDef ret;

    if (dev == 0 || lux_x10 == 0) return HAL_ERROR;
    (void)VEML7700_SetGain(dev, gains[gi]);
    (void)VEML7700_SetIntegrationTime(dev, its[ii]);
    ret = VEML7700_ReadALSRaw(dev, &raw);
    if (ret != HAL_OK) return ret;

    if (raw <= 100U) {
        while ((raw <= 100U) && !((gi == 3U) && (ii == 5U))) {
            if (gi < 3U) gi++;
            else if (ii < 5U) ii++;
            (void)VEML7700_SetGain(dev, gains[gi]);
            (void)VEML7700_SetIntegrationTime(dev, its[ii]);
            ret = VEML7700_ReadALSRaw(dev, &raw);
            if (ret != HAL_OK) return ret;
        }
    } else {
        corrected = 1U;
        while ((raw > 10000U) && (ii > 0U)) {
            ii--;
            (void)VEML7700_SetIntegrationTime(dev, its[ii]);
            ret = VEML7700_ReadALSRaw(dev, &raw);
            if (ret != HAL_OK) return ret;
        }
    }

    *lux_x10 = veml_compute_lux_x10(dev, raw, corrected);
    return HAL_OK;
}
