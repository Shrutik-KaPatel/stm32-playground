#include "LIS3DSHTR.h"

void LIS3_WriteReg(LIS3_HandleTypeDef *hlis, uint8_t reg, uint8_t data)
{
	 uint8_t tx[2] = { reg & 0x7F, data };
	 HAL_GPIO_WritePin(hlis->cs_port, hlis->cs_pin, GPIO_PIN_RESET);
	 HAL_SPI_Transmit(hlis->hspi, tx, 2, HAL_MAX_DELAY);
	 HAL_GPIO_WritePin(hlis->cs_port, hlis->cs_pin, GPIO_PIN_SET);
}

uint8_t LIS3_ReadReg(LIS3_HandleTypeDef *hlis, uint8_t reg)
{
	 uint8_t tx = reg | 0x80;
	 uint8_t rx = 0;
	 HAL_GPIO_WritePin(hlis->cs_port, hlis->cs_pin, GPIO_PIN_RESET);
	 HAL_SPI_Transmit(hlis->hspi, &tx, 1, HAL_MAX_DELAY);
	 HAL_SPI_Receive(hlis->hspi, &rx, 1, HAL_MAX_DELAY);
	 HAL_GPIO_WritePin(hlis->cs_port, hlis->cs_pin, GPIO_PIN_SET);
	 return rx;
}

void LIS3_Init(LIS3_HandleTypeDef *hlis)
{
	LIS3_WriteReg(hlis, LIS3_CTRL_REG4, LIS3_CTRL_REG4_100HZ_XYZ);
	HAL_Delay(10);
}
void LIS3_ReadXYZ(LIS3_HandleTypeDef *hlis, LIS3_DataTypeDef *data)
{
	data->x = (int16_t)(LIS3_ReadReg(hlis, LIS3_OUT_X_H) << 8 | LIS3_ReadReg(hlis, LIS3_OUT_X_L) );
	data->y = (int16_t)(LIS3_ReadReg(hlis, LIS3_OUT_Y_H) << 8 | LIS3_ReadReg(hlis, LIS3_OUT_Y_L) );
	data->z = (int16_t)(LIS3_ReadReg(hlis, LIS3_OUT_Z_H) << 8 | LIS3_ReadReg(hlis, LIS3_OUT_Z_L) );
}
