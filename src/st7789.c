/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 */

#include <string.h>

#include "hardware/gpio.h"

#include "pico/st7789.h"
#include "pico/qrcodegen.h"
#include "pico/fonts.h"

static struct st7789_config st7789_cfg;
static uint16_t st7789_width;
static uint16_t st7789_height;
static bool st7789_data_mode = false;

static void st7789_cmd(uint8_t cmd, const uint8_t* data, size_t len)
{
    if (CS_PIN > -1) {
        spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    } else {
        spi_set_format(SPI_PORT, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    }
    st7789_data_mode = false;

    sleep_us(1);
    if (CS_PIN > -1) {
        gpio_put(CS_PIN, 0);
    }
    gpio_put(DC_PIN, 0);
    sleep_us(1);
    
    spi_write_blocking(SPI_PORT, &cmd, sizeof(cmd));
    
    if (len) {
        sleep_us(1);
        gpio_put(DC_PIN, 1);
        sleep_us(1);
        
        spi_write_blocking(SPI_PORT, data, len);
    }

    sleep_us(1);
    if (CS_PIN > -1) {
        gpio_put(CS_PIN, 1);
    }
    gpio_put(DC_PIN, 1);
    sleep_us(1);
}

void st7789_caset(uint16_t xs, uint16_t xe)
{
    uint8_t data[] = {
        xs >> 8,
        xs & 0xff,
        xe >> 8,
        xe & 0xff,
    };

    // CASET (2Ah): Column Address Set
    st7789_cmd(0x2a, data, sizeof(data));
}

void st7789_raset(uint16_t ys, uint16_t ye)
{
    uint8_t data[] = {
        ys >> 8,
        ys & 0xff,
        ye >> 8,
        ye & 0xff,
    };

    // RASET (2Bh): Row Address Set
    st7789_cmd(0x2b, data, sizeof(data));
}

void st7789_init(const struct st7789_config* config, uint16_t width, uint16_t height)
{

     st7789_width = width;
    st7789_height = height;
    
    memcpy(&st7789_cfg, config, sizeof(st7789_cfg));
    st7789_width = width;
    st7789_height = height;

    spi_init(SPI_PORT, 125 * 1000 * 1000);
    if (CS_PIN > -1) {
        spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    } else {
        spi_set_format(SPI_PORT, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    }

    gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SCK_PIN, GPIO_FUNC_SPI);

    if (CS_PIN > -1) {
        gpio_init(CS_PIN);
    }
    gpio_init(DC_PIN);
    gpio_init(RST_PIN);
    gpio_init(BK_PIN);

    if (CS_PIN > -1) {
        gpio_set_dir(CS_PIN, GPIO_OUT);
    }
    gpio_set_dir(DC_PIN, GPIO_OUT);
    gpio_set_dir(RST_PIN, GPIO_OUT);
    gpio_set_dir(BK_PIN, GPIO_OUT);

    if (CS_PIN > -1) {
        gpio_put(CS_PIN, 1);
    }
    gpio_put(DC_PIN, 1);
    gpio_put(RST_PIN, 1);
    sleep_ms(100);
    
    // SWRESET (01h): Software Reset
    st7789_cmd(0x01, NULL, 0);
    sleep_ms(150);

    // SLPOUT (11h): Sleep Out
    st7789_cmd(0x11, NULL, 0);
    sleep_ms(50);

    // COLMOD (3Ah): Interface Pixel Format
    // - RGB interface color format     = 65K of RGB interface
    // - Control interface color format = 16bit/pixel
    st7789_cmd(0x3a, (uint8_t[]){ 0x55 }, 1);
    sleep_ms(10);

    // MADCTL (36h): Memory Data Access Control
    // - Page Address Order            = Top to Bottom
    // - Column Address Order          = Left to Right
    // - Page/Column Order             = Normal Mode
    // - Line Address Order            = LCD Refresh Top to Bottom
    // - RGB/BGR Order                 = RGB
    // - Display Data Latch Data Order = LCD Refresh Left to Right
    st7789_cmd(0x36, (uint8_t[]){ 0x00 }, 1);
   
    st7789_caset(0, width);
    st7789_raset(0, height);

    // INVON (21h): Display Inversion On
    st7789_cmd(0x21, NULL, 0);
    sleep_ms(10);

    // NORON (13h): Normal Display Mode On
    st7789_cmd(0x13, NULL, 0);
    sleep_ms(10);

    // DISPON (29h): Display On
    st7789_cmd(0x29, NULL, 0);
    sleep_ms(10);

    gpio_put(BK_PIN, 1);
    
}

void st7789_ramwr()
{
    sleep_us(1);
    if (CS_PIN > -1) {
        gpio_put(CS_PIN, 0);
    }
    gpio_put(DC_PIN, 0);
    sleep_us(1);

    // RAMWR (2Ch): Memory Write
    uint8_t cmd = 0x2c;
    spi_write_blocking(SPI_PORT, &cmd, sizeof(cmd));

    sleep_us(1);
    if (CS_PIN > -1) {
        gpio_put(CS_PIN, 0);
    }
    gpio_put(DC_PIN, 1);
    sleep_us(1);
}

void st7789_write(const void* data, size_t len)
{
    if (!st7789_data_mode) {
        st7789_ramwr();

        if (CS_PIN > -1) {
            spi_set_format(SPI_PORT, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        } else {
            spi_set_format(SPI_PORT, 16, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
        }

        st7789_data_mode = true;
    }

    spi_write16_blocking(SPI_PORT, data, len / 2);
}

void st7789_put(uint16_t pixel)
{
    st7789_write(&pixel, sizeof(pixel));
}

void st7789_fill(uint16_t pixel)
{
    int num_pixels = DISP_WIDTH * DISP_HEIGHT;

    st7789_set_cursor(0, 0);

    for (int i = 0; i < num_pixels; i++) {
        st7789_put(pixel);
    }
}


void st7789_set_cursor(uint16_t x, uint16_t y)
{
    st7789_caset(x, DISP_WIDTH);
    st7789_raset(y, DISP_HEIGHT);
}

void st7789_vertical_scroll(uint16_t row)
{
    uint8_t data[] = {
        (row >> 8) & 0xff,
        row & 0x00ff
    };

    // VSCSAD (37h): Vertical Scroll Start Address of RAM 
    st7789_cmd(0x37, data, sizeof(data));
}



void st7789_writeString(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor)
{
      st7789_set_cursor(100,100);
	while (*str) {
		if (x + font.width >= 240) {
			x = 0;
			y += font.height;
			if (y + font.height >= 320) {
				break;
			}

			if (*str == ' ') {
				// skip spaces in the beginning of the new line
				str++;
				continue;
			}
		}
		writeChar(x, y, *str, font, color, bgcolor);
		x += font.width;
		str++;
	}
}


//Initialize display
void display_init(){
    soft_reset();
    sleep_mode(false);

    //_set_color_mode (COLOR_MODE_65K | COLOR_MODE_16BIT);
    _set_color_mode(COLOR_MODE_16BIT);
    sleep_ms(50);
    rotation(1);
    inversion_mode(true);
    sleep_ms(10);
    _writeCommand(ST7789_NORON);
    sleep_ms(10);
    _writeCommand(ST7789_SLPOUT);
     sleep_ms(10);

    _writeCommand(ST7789_DISPON);
   
    sleep_ms(500);
    _set_window(0, 0, DISP_WIDTH, DISP_HEIGHT);
    ST7789_Fill(BLACK);

}




 void setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	uint16_t x_start = x0, x_end = x1;
	uint16_t y_start = y0, y_end = y1;
	
	/* Column Address set */
	_writeCommand(ST7789_CASET); 
	{
		uint8_t data[] = {x_start >> 8, x_start & 0xFF, x_end >> 8, x_end & 0xFF};
		_writeData(data, sizeof(data));
	}

	/* Row Address set */
	_writeCommand(ST7789_RASET);
	{
		uint8_t data[] = {y_start >> 8, y_start & 0xFF, y_end >> 8, y_end & 0xFF};
		_writeData(data, sizeof(data));
	}
	/* Write to RAM */
	_writeCommand(ST7789_RAMWR);
}

void ST7789_Fill(uint16_t color)
{
	uint16_t i;
	setAddressWindow(0, 0, DISP_WIDTH - 1, DISP_HEIGHT - 1);

		uint16_t j;
		for (i = 0; i < DISP_WIDTH; i++)
				for (j = 0; j < DISP_HEIGHT; j++) {
					uint8_t data[] = {color >> 8, color & 0xFF};
					_writeData(data, sizeof(data));
				}

}

void _writeCommand(uint8_t command){
    gpio_put(CS_PIN, 0);
    gpio_put(DC_PIN, 0);
    spi_write_blocking(SPI_PORT, &command, 1);
    gpio_put(CS_PIN, 1);
}

void _writeData(uint8_t data[], uint32_t Len){
    gpio_put(CS_PIN, 0);
    gpio_put(DC_PIN, 1);
    spi_write_blocking(SPI_PORT, data, Len);
    gpio_put(CS_PIN, 1);
}

void soft_reset()   // Soft reset display.
{
    _writeCommand(ST7789_SWRESET);
    sleep_ms(150);
}

void sleep_mode(bool value) //Enable(1) or disable(0) display sleep mode. 
{
    if(value){
        _writeCommand(ST7789_SLPIN);
    }else{
        _writeCommand(ST7789_SLPOUT);
    }
}

void inversion_mode(int value)  //Enable(1) or disable(0) display inversion mode.
{
    if(value){
        _writeCommand(ST7789_INVON);
    }else{
        _writeCommand(ST7789_INVOFF);
    }
}

void _set_color_mode(uint8_t mode)  //Set display color mode.
{
    _writeCommand(ST7789_COLMOD);
    uint8_t data[] = { mode, 0x77 };
    _writeData(data, 2);
}


 void ST7789_WriteSmallData(uint8_t data)
{

	   _writeData(&data, 1);

}

void rotation(uint8_t m)
{
	_writeCommand(ST7789_MADCTL);	// MADCTL
	switch (m) {
	case 0:
		ST7789_WriteSmallData(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
		break;
	case 1:
		ST7789_WriteSmallData(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
		break;
	case 2:
		ST7789_WriteSmallData(ST7789_MADCTL_RGB);
		break;
	case 3:
		ST7789_WriteSmallData(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
		break;
	default:
		break;
	}
}

void _set_columns(int start, int end){
    //Send CASET (column address set) command to display.
    //Args: start (int): column start address end (int): column end address
    if(start > end > DISP_WIDTH) return;
    uint8_t _x = start;
    uint8_t _y = end;
    uint8_t result[] = { 0x00, 0x00, _x, _y };
    _writeCommand(ST7789_CASET);
    _writeData(result, 5);
}

void _set_rows(int start, int end){
    //Send RASET (row address set) command to display.
    //Args: start (int): row start address, end (int): row end address
    if(start > end > DISP_HEIGHT) return;
    _writeCommand(ST7789_RASET);
    uint8_t _x = start;
    uint8_t _y = end;
    uint8_t result[] = { 0x00, 0x00, _x, _y };
    _writeData(result, 5);
}

void _set_window(int x0, int y0, int x1, int y1){;
    //Set window to column and row address.
    //Args:x0 (int): column start address, y0 (int): row start address
    //x1 (int): column end address, y1 (int): row end address
    _set_columns(x0, x1);
    _set_rows(y0, y1);
    _writeCommand(ST7789_RAMWR);
}

void vline(int x, int y, int length, uint16_t color){
    //Draw vertical line at the given location and color.
    //Args: x (int): x coordinate, Y (int): y coordinate 
    //length (int): length of line, color (int): 565 encoded color
    fill_rect(x, y, 1, length, color);
}

void ST7789_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
	if ((x >= DISP_WIDTH) || (y >= DISP_HEIGHT))
		return;
	if ((x + w - 1) >= DISP_WIDTH)
		return;
	if ((y + h - 1) >= DISP_HEIGHT)
		return;


	setAddressWindow(x, y, x + w - 1, y + h - 1);
	_writeData((uint8_t *)data, sizeof(uint16_t) * w * h);

}

void hline(int x, int y, int length, uint16_t color){
    //Draw horizontal line at the given location and color.
    //Args:x (int): x coordinate, Y (int): y coordinate
    //length (int): length of line, color (int): 565 encoded color
    fill_rect(x, y, length, 1, color);
}

void blit_buffer(uint8_t buffer[], uint8_t bufferLen,int x, int y, int width, int height){
    //Copy buffer to display at the given location.
    //Args:buffer (bytes): Data to copy to display
    //x (int): Top left corner x coordinate, Y (int): Top left corner y coordinate
    //width (int): Width, height (int): Height
    _set_window(x, y, x + width - 1, y + height - 1);
    _writeData(buffer, bufferLen);
}

void rect(int x, int y, int w, int h, uint16_t color){
    //Draw a rectangle at the given location, size and color.
    //Args: x (int): Top left corner x coordinate, y (int): Top left corner y coordinate
    //width (int): Width in pixels, height (int): Height in pixels
    //color (int): 565 encoded color
    hline(x, y, w, color);
    vline(x, y, h, color);
    vline(x + w - 1, y, h, color);
    hline(x, y + h - 1, w, color);
}

void fill_rect(int x, int y, int width, int height, uint16_t color){
    //Draw a rectangle at the given location, size and filled with color.
    //Args: x (int): Top left corner x coordinate, y (int): Top left corner y coordinate
    //width (int): Width in pixels, height (int): Height in pixels
    //color (int): 565 encoded color
    _set_window(x, y, x + width - 1, y + height - 1);

    //uint16_t pixel16[1];
    //pixel16[0] = color;
    uint8_t pixel[2];
    pixel[0]=(color >> 8);
    pixel[1]=color & 0xff;

    div_t output;
    output = div(width *= height, _BUFFER_SIZE);
    int chunks = output.quot;

    gpio_put(DC_PIN, 1);    //Open to write
    uint sendbuffer = _BUFFER_SIZE * 2;
    uint8_t _drawpixel[sendbuffer];

    for (int i = 0; i < sendbuffer; i++)
    {
        _drawpixel[i] = pixel[i%2];
    }

    for(int i=_BUFFER_SIZE; i!=0; i--){
        _writeData(_drawpixel, sendbuffer);
    }
}

void fill(uint16_t color){
    //Fill the entire FrameBuffer with the specified color.
    //Args:color (int): 565 encoded color
    fill_rect(0, 0, DISP_WIDTH, DISP_HEIGHT, color);
}

void line(int x0, int y0, int x1, int y1, uint16_t color){
    //Draw a single pixel wide line starting at x0, y0 and ending at x1, y1.
    /*Args:
            x0 (int): Start point x coordinate
            y0 (int): Start point y coordinate
            x1 (int): End point x coordinate
            y1 (int): End point y coordinate
            color (int): 565 encoded color*/
    int a = abs(y1 - y0);
    int b = abs(x1 - x0);
    int steep = (a > b);
    if(steep){
        x0, y0 = y0, x0;
        x1, y1 = y1, x1;
    }

    if(x0 > x1){
        x0, x1 = x1, x0;
        y0, y1 = y1, y0;
    }
    int dx = x1 - x0;
    int dy = abs(y1 - y0);
    int err = dx / 2;

    int ystep;
    if(y0 < y1){
        ystep = 1;
    }else{
        ystep = -1;
    }

    while(x0 <= x1){
        if(steep){
            pixel(y0, x0, color);
        }else{
            pixel(x0, y0, color);
        }
        err -= dy;
        if(err < 0){
            y0 += ystep;
            err += dx;
        }
        x0 += 1;
    }
}

void vscrdef(int tfa, int vsa, int bfa){
    /*Set Vertical Scrolling Definition.
        To scroll a 135x240 display these values should be 40, 240, 40.
        There are 40 lines above the display that are not shown followed by
        240 lines that are shown followed by 40 more lines that are not shown.
        You could write to these areas off display and scroll them into view by
        changing the TFA, VSA and BFA values.
        Args:
            tfa (int): Top Fixed Area
            vsa (int): Vertical Scrolling Area
            bfa (int): Bottom Fixed Area*/
            //\x3e\x48\x48\x48
    uint8_t _vscrdef[] = { 0x3E, 0x48, 0x48, 0x48, tfa, vsa, bfa };
    _writeCommand(ST7789_VSCRDEF);
    _writeData(_vscrdef, 7);
}

void vscsad(int vssa){
    uint8_t _vscsad[] = { 0x3E, 0x48, vssa };
    _writeCommand(ST7789_VSCSAD);
    _writeData(_vscsad, 3);
}

uint16_t convert_rgb_to_hex(int red, int green, int blue){
    //Convert red, green and blue values (0-255) into a 16-bit 565 encoding
    return (red & 0xf8) << 8 | (green & 0xfc) << 3 | blue >> 3;
}

void writeChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor)
{
	uint32_t i, b, j;
	setAddressWindow(x, y, x + font.width - 1, y + font.height - 1);

	for (i = 0; i < font.height; i++) {
		b = font.data[(ch - 32) * font.height + i];
		for (j = 0; j < font.width; j++) {
			if ((b << j) & 0x8000) {
				uint8_t data[] = {color >> 8, color & 0xFF};
				_writeData(data, sizeof(data));
			}
			else {
				uint8_t data[] = {bgcolor >> 8, bgcolor & 0xFF};
				_writeData(data, sizeof(data));
			}
		}
	}
}

void writeString(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor)
{
	while (*str) {
		if (x + font.width >= DISP_WIDTH) {
			x = 0;
			y += font.height;
			if (y + font.height >= DISP_HEIGHT) {
				break;
			}

			if (*str == ' ') {
				// skip spaces in the beginning of the new line
				str++;
				continue;
			}
		}
		writeChar(x, y, *str, font, color, bgcolor);
		x += font.width;
		str++;
	}
}

