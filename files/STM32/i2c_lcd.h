/*
 * i2c_lcd.h
 *
 *  Created on: Jul 25, 2024
 *      Author: iot
 */

#ifndef INC_I2C_LCD_H_
#define INC_I2C_LCD_H_

#include "main.h"

void lcd_init(void);
void lcd_send_cmd(char cmd);
void lcd_send_data(char data);
void lcd_send_string(char *str);
void lcd_put_cur(int row, int col);
void lcd_clear(void);

#endif /* INC_I2C_LCD_H_ */

