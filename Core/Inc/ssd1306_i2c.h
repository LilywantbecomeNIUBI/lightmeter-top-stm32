#ifndef SSD1306_I2C_H
#define SSD1306_I2C_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_WIDTH       128U
#define SSD1306_HEIGHT      64U
#define SSD1306_I2C_ADDR    (0x3CU << 1)

HAL_StatusTypeDef SSD1306_Init(I2C_HandleTypeDef *hi2c);
void SSD1306_Clear(void);
void SSD1306_Update(void);
void SSD1306_DrawChar(uint8_t x, uint8_t page, char c);
void SSD1306_DrawString(uint8_t x, uint8_t page, const char *str);
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
void SSD1306_DrawHLine(uint8_t x, uint8_t y, uint8_t w, uint8_t color);
void SSD1306_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);

#ifdef __cplusplus
}
#endif

#endif
