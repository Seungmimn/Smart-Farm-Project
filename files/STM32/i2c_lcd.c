/*
 * i2c_lcd.c
 *
 *  Created on: Jul 25, 2024
 *      Author: iot
 */

#include "i2c_lcd.h"

extern I2C_HandleTypeDef hi2c1;  // 사용 중인 I2C 핸들러를 선언합니다.

#define SLAVE_ADDRESS_LCD 0x4E  // LCD의 I2C 주소입니다.

void lcd_send_cmd(char cmd)
{
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (cmd & 0xf0);
    data_l = ((cmd << 4) & 0xf0);
    data_t[0] = data_u | 0x0C;  // En=1, Rs=0
    data_t[1] = data_u | 0x08;  // En=0, Rs=0
    data_t[2] = data_l | 0x0C;  // En=1, Rs=0
    data_t[3] = data_l | 0x08;  // En=0, Rs=0
    HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, (uint8_t *) data_t, 4, 100);
}

void lcd_send_data(char data)
{
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (data & 0xf0);
    data_l = ((data << 4) & 0xf0);
    data_t[0] = data_u | 0x0D;  // En=1, Rs=1
    data_t[1] = data_u | 0x09;  // En=0, Rs=1
    data_t[2] = data_l | 0x0D;  // En=1, Rs=1
    data_t[3] = data_l | 0x09;  // En=0, Rs=1
    HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, (uint8_t *) data_t, 4, 100);
}

void lcd_clear(void)
{
    lcd_send_cmd(0x80);
    for (int i = 0; i < 70; i++)
    {
        lcd_send_data(' ');
    }
}

void lcd_put_cur(int row, int col)
{
    switch (row)
    {
    case 0:
        col |= 0x80;
        break;
    case 1:
        col |= 0xC0;
        break;
    }
    lcd_send_cmd(col);
}

void lcd_init(void)
{
    HAL_Delay(50);  // wait for >40ms
    lcd_send_cmd(0x30);
    HAL_Delay(5);   // wait for >4.1ms
    lcd_send_cmd(0x30);
    HAL_Delay(1);   // wait for >100us
    lcd_send_cmd(0x30);
    HAL_Delay(10);
    lcd_send_cmd(0x20);  // 4bit mode
    HAL_Delay(10);

    // dislay initialisation
    lcd_send_cmd(0x28);  // Function set --> DL=0 (4-bit mode), N=1 (2 line display), F=0 (5x8 characters)
    HAL_Delay(1);
    lcd_send_cmd(0x08);  // Display on/off control --> D=0, C=0, B=0 --> display off
    HAL_Delay(1);
    lcd_send_cmd(0x01);  // clear display
    HAL_Delay(1);
    HAL_Delay(1);
    lcd_send_cmd(0x06);  // Entry mode set --> I/D=1 (increment cursor) & S=0 (no shift)
    HAL_Delay(1);
    lcd_send_cmd(0x0C);  // Display on/off control --> D=1, C=0, B=0 --> display on, cursor off, blink off
}

void lcd_send_string(char *str)
{
    while (*str) lcd_send_data(*str++);
}


