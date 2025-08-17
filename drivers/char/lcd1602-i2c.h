#ifndef _LCD1602_I2C_H_
#define _LCD1602_I2C_H_

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>                   //file_operations
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>                 //kmalloc()
#include <linux/uaccess.h>              //copy_to/from_user()
#include <linux/ioctl.h>
#include <linux/err.h>

#define USE_SMBUS

static int i2c_master_write_byte(struct i2c_client *client, u8 value) {
#ifdef USE_SMBUS // Use SMBus API
    return i2c_smbus_write_byte(client, value);
#else // Use plain I2C API
    u8 buf[1];
    buf[0] = value;
    return i2c_master_send(client, buf, sizeof(buf));
#endif
}

#define LCD1602_ADDR 0x27 //I2C address of LCD1602
#define LCD_CMD 0   // RS = 0
#define LCD_DATA 1  // RS = 1

char lcd_str[32];
int pos = 0;

static struct i2c_client *lcd1602_client;

static int lcd1602_send(uint8_t value, uint8_t mode)
{
    uint8_t high_nibble = value & 0xF0;
    uint8_t low_nibble = (value << 4) & 0xF0;
    int ret;

	uint8_t high_nibble_mode = high_nibble | mode;
    uint8_t low_nibble_mode = low_nibble | mode;

    ret = i2c_master_write_byte(lcd1602_client, high_nibble_mode | 0x0C); // EN = 1 (C = 1 1 0 0)
    if (ret < 0) return ret;
    ret = i2c_master_write_byte(lcd1602_client, high_nibble_mode | 0x08); // EN = 0 (8 = 1 0 0 0)
    if (ret < 0) return ret;

    ret = i2c_master_write_byte(lcd1602_client, low_nibble_mode | 0x0C); // EN = 1 (C = 1 1 0 0)
    if (ret < 0) return ret;
    ret = i2c_master_write_byte(lcd1602_client, low_nibble_mode | 0x08); // EN = 0 (8 = 1 0 0 0)

    msleep(1);

    return ret;
}

static int lcd1602_command_4bit(uint8_t value)
{
    uint8_t nibble_mode = (value << 4) | LCD_CMD;
    int ret;

    ret = i2c_smbus_write_byte(lcd1602_client, nibble_mode | 0x0C); // EN = 1 (C = 1 1 0 0)
    if (ret < 0) return ret;
    ret = i2c_smbus_write_byte(lcd1602_client, nibble_mode | 0x08); // EN = 0 (8 = 1 0 0 0)

    return ret;
}

static int lcd1602_command(uint8_t cmd)
{
    return lcd1602_send(cmd, LCD_CMD);
}

static int lcd1602_data(uint8_t data)
{
    return lcd1602_send(data, LCD_DATA);
}

static int lcd1602_home(void) {
    int ret;
    ret = lcd1602_command(0x02);
    msleep(2);
    return ret;
}

static int lcd1602_off(void) {
    return lcd1602_command(0x08);
}

static int lcd1602_clear(void) {
    return lcd1602_command(0x01);
}

static int lcd1602_gotoXY(int row, int col) {
	uint8_t pos_Addr;
    int ret;

	if(row == 1) {
		pos_Addr = 0x80 + row - 1 + col;
	} else {
		pos_Addr = 0x80 | (0x40 + col);
	}
	
    ret = lcd1602_command(pos_Addr);
    return ret;
}

static int lcd1602_print(char *str) {
    int ret = 0;
    int _pos = 0;

    lcd1602_clear();

    while (*str) {
        ret = lcd1602_data(*str++);

        _pos++;
        if(_pos == 16)
            lcd1602_gotoXY(2,0);

        if(ret)
            return ret;
    }
    return ret;
}

static void lcd1602_init(void)
{
    // Initialization sequence for LCD1602
    msleep(50); // Wait for LCD to power up
	lcd1602_command(0x00);
	msleep(1000);

	// Try to set 4 bit mode
    lcd1602_command_4bit(0x3);
    msleep(5);
    lcd1602_command_4bit(0x3);
    msleep(1);
    lcd1602_command_4bit(0x3);

    lcd1602_command_4bit(0x2);
    lcd1602_command(0x28); // Function Set: 4-bit mode, 2 lines, 5x8 dots
    lcd1602_command(0x08); // Display off
    lcd1602_command(0x01); // Clear display
    lcd1602_command(0x06); // Entry mode set: increment cursor

    lcd1602_command(0x0C); // Display on, cursor off, blink off
}

#endif /* _LCD1602_DRV_H_ */