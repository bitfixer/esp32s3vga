#include "VGA.h"
#include <esp_log.h>
#include <esp_lcd_panel_ops.h>
#include <string.h>

#define EXAMPLE_PIN_NUM_HSYNC          4
#define EXAMPLE_PIN_NUM_VSYNC          3
#define EXAMPLE_PIN_NUM_DE             0
#define EXAMPLE_PIN_NUM_PCLK           9
#define EXAMPLE_PIN_NUM_DATA0          14
#define EXAMPLE_PIN_NUM_DATA1          13
#define EXAMPLE_PIN_NUM_DATA2          12
#define EXAMPLE_PIN_NUM_DATA3          11
#define EXAMPLE_PIN_NUM_DATA4          10
#define EXAMPLE_PIN_NUM_DATA5          39
#define EXAMPLE_PIN_NUM_DATA6          38
#define EXAMPLE_PIN_NUM_DATA7          45
#define EXAMPLE_PIN_NUM_DISP_EN        -1

static const char *TAG = "vga";

bool VGA::init(int width, int height, int scale, int bits, int* pins, bool usePsram) {

	// temp replace
	_frameScale = scale;
	_frameWidth = width / _frameScale;
	_frameHeight = height / _frameScale;
	_screenWidth = width;
	_screenHeight = height;
    _colorBits = bits;
    _bounceBufferLines = height / 10;

    int pixelClockHz = 0;
    if (width == 800 && height == 600) {
        pixelClockHz = 40000000;
    } else if (width == 640 && height == 480) {
        pixelClockHz = 36000000;
    } else if (width == 640 && height == 400) {
        pixelClockHz = 25000000;
    } else if (width == 640 && height == 350) {
        pixelClockHz = 25000000;
    }

	ESP_LOGI(TAG, "Create semaphores");
    _sem_vsync_end = xSemaphoreCreateBinary();
    assert(_sem_vsync_end);
    _sem_gui_ready = xSemaphoreCreateBinary();
    assert(_sem_gui_ready);

	ESP_LOGI(TAG, "Install RGB LCD panel driver");
    esp_lcd_rgb_panel_config_t panel_config;
    memset(&panel_config, 0, sizeof(esp_lcd_rgb_panel_config_t));

    panel_config.data_width = 8;
    panel_config.psram_trans_align = 64,
    panel_config.num_fbs = 0;
    panel_config.clk_src = LCD_CLK_SRC_PLL240M;
    panel_config.bounce_buffer_size_px = _bounceBufferLines * _screenWidth;
    panel_config.disp_gpio_num = -1;
    panel_config.pclk_gpio_num = EXAMPLE_PIN_NUM_PCLK;
    panel_config.vsync_gpio_num = EXAMPLE_PIN_NUM_VSYNC;
    panel_config.hsync_gpio_num = EXAMPLE_PIN_NUM_HSYNC;
    panel_config.de_gpio_num = 0;

    // check for pin definitions
    if (pins) {
        for (int i = 0; i < 8; i++) {
            if (i < bits) {
                panel_config.data_gpio_nums[i] = pins[i];
            } else {
                panel_config.data_gpio_nums[i] = -1;
            }
        }
    } else {
        panel_config.data_gpio_nums[0] = EXAMPLE_PIN_NUM_DATA0;
        panel_config.data_gpio_nums[1] = EXAMPLE_PIN_NUM_DATA1;
        panel_config.data_gpio_nums[2] = EXAMPLE_PIN_NUM_DATA2;
        panel_config.data_gpio_nums[3] = EXAMPLE_PIN_NUM_DATA3;
        panel_config.data_gpio_nums[4] = EXAMPLE_PIN_NUM_DATA4;
        panel_config.data_gpio_nums[5] = EXAMPLE_PIN_NUM_DATA5;
        panel_config.data_gpio_nums[6] = EXAMPLE_PIN_NUM_DATA6;
        panel_config.data_gpio_nums[7] = EXAMPLE_PIN_NUM_DATA7;
    }
        
    panel_config.timings.pclk_hz = pixelClockHz;
    panel_config.timings.h_res = _screenWidth;
    panel_config.timings.v_res = _screenHeight;
    
    if (width == 800 && height == 600) {
        panel_config.timings.hsync_back_porch = 88;
        panel_config.timings.hsync_front_porch = 40;
        panel_config.timings.hsync_pulse_width = 128;
        panel_config.timings.vsync_back_porch = 23;
        panel_config.timings.vsync_front_porch = 1;
        panel_config.timings.vsync_pulse_width = 4;
    } else if (width == 640 && height == 480) {
        panel_config.timings.hsync_back_porch = 80;
        panel_config.timings.hsync_front_porch = 56;
        panel_config.timings.hsync_pulse_width = 56;
        panel_config.timings.vsync_back_porch = 25;
        panel_config.timings.vsync_front_porch = 1;
        panel_config.timings.vsync_pulse_width = 3;
    } else if (width == 640 && height == 400) {
        panel_config.timings.hsync_back_porch = 48;
        panel_config.timings.hsync_front_porch = 16;
        panel_config.timings.hsync_pulse_width = 96;
        panel_config.timings.vsync_back_porch = 35;
        panel_config.timings.vsync_front_porch = 12;
        panel_config.timings.vsync_pulse_width = 2;
    } else if (width == 640 && height == 350) {
        panel_config.timings.hsync_back_porch = 48;
        panel_config.timings.hsync_front_porch = 16;
        panel_config.timings.hsync_pulse_width = 96;
        panel_config.timings.vsync_back_porch = 60;
        panel_config.timings.vsync_front_porch = 37;
        panel_config.timings.vsync_pulse_width = 2;
    }

    panel_config.timings.flags.pclk_active_neg = true;
    panel_config.timings.flags.hsync_idle_low = 0;
    panel_config.timings.flags.vsync_idle_low = 0;

    panel_config.flags.fb_in_psram = 0;
    panel_config.flags.double_fb = 0;
    panel_config.flags.no_fb = 1;

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &_panel_handle));

    // allocate frame buffers in memory
    int fbSize = _frameWidth*_frameHeight;
    if (_colorBits == 3) {
        fbSize /= 2;
    }
    for (int i = 0; i < 2; i++) {
        if (usePsram) {
             ESP_LOGI(TAG, "allocating in spi ram");
            _frameBuffers[i] = (uint8_t*)heap_caps_malloc(fbSize, MALLOC_CAP_SPIRAM);
        } else {
             ESP_LOGI(TAG, "allocating in main memory");
            _frameBuffers[i] = (uint8_t*)malloc(fbSize);
        }
        assert(_frameBuffers[i]);
    }

    memset(_frameBuffers[0], 255, _frameWidth*_frameHeight);

    ESP_LOGI(TAG, "Register event callbacks");
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
    	.on_vsync = vsyncEvent,
        .on_bounce_empty = bounceEvent,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(_panel_handle, &cbs, this));

    ESP_LOGI(TAG, "Initialize RGB LCD panel");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(_panel_handle));
    ESP_LOGI(TAG, "Init complete");

    return true;
}

void VGA::vsyncWait() {
	// get draw semaphore
    xSemaphoreGive(_sem_gui_ready);
    xSemaphoreTake(_sem_vsync_end, portMAX_DELAY);
}

uint8_t* VGA::getDrawBuffer() {
	if (_frameBufferIndex == 0) {
		return _frameBuffers[1];
	} else {
		return _frameBuffers[0];
	}
}

bool VGA::vsyncEvent(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx) {
	return true;
}

bool VGA::bounceEvent(esp_lcd_panel_handle_t panel, void* bounce_buf, int pos_px, int len_bytes, void* user_ctx) {
	VGA* vga = (VGA*)user_ctx;

    int div = vga->_frameScale * vga->_frameScale;
    int pixelsPerByte = 1;
    if (vga->_colorBits == 3) {
        pixelsPerByte = 2;
    }
    uint8_t* pptr = vga->_frameBuffers[vga->_frameBufferIndex] + (pos_px / (div*pixelsPerByte));
    int pixelsToCopy = len_bytes / div;
    int lines = pixelsToCopy / vga->_frameWidth;

    if (vga->_frameScale == 1 && vga->_colorBits == 8) {
        // just copy the bytes
        uint8_t* bbptr = (uint8_t*)bounce_buf;
        memcpy(bbptr, pptr, len_bytes);
    } else if (vga->_frameScale == 1 && vga->_colorBits == 3) {
        uint8_t* bbptr = (uint8_t*)bounce_buf;
        uint8_t pixelBits;
        for (int y = 0; y < lines; y++) {
            for (int x = 0; x < vga->_frameWidth; x += 10) {
                // partially unrolled loop for speed
                pixelBits = *pptr;
                *(bbptr++) = pixelBits & 0b00000111;
                pixelBits >>= 4;
                *(bbptr++) = pixelBits & 0b00000111;
                pptr++;
                pixelBits = *pptr;
                *(bbptr++) = pixelBits & 0b00000111;
                pixelBits >>= 4;
                *(bbptr++) = pixelBits & 0b00000111;
                pptr++;
                pixelBits = *pptr;
                *(bbptr++) = pixelBits & 0b00000111;
                pixelBits >>= 4;
                *(bbptr++) = pixelBits & 0b00000111;
                pptr++;
                pixelBits = *pptr;
                *(bbptr++) = pixelBits & 0b00000111;
                pixelBits >>= 4;
                *(bbptr++) = pixelBits & 0b00000111;
                pptr++;
                pixelBits = *pptr;
                *(bbptr++) = pixelBits & 0b00000111;
                pixelBits >>= 4;
                *(bbptr++) = pixelBits & 0b00000111;
                pptr++;
            }
        }
    } else if (vga->_frameScale == 2 && vga->_colorBits == 8) {
        uint8_t* bbptr = ( uint8_t*)bounce_buf;
        for (int y = 0; y < lines; y++) {
            uint8_t* lineptr = bbptr;
            for (int x = 0; x < vga->_frameWidth; x += 40) {
                // partially unrolled loop for speed
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
                *(bbptr++) = *pptr;
                *(bbptr++) = *(pptr++);
            }
            memcpy(bbptr, lineptr, vga->_screenWidth);
            bbptr += vga->_screenWidth;
        }
    } else if (vga->_frameScale == 2 && vga->_colorBits == 3) {
        uint8_t* bbptr = (uint8_t*)bounce_buf;
        uint8_t pixelBits;
        for (int y = 0; y < lines; y++) {
            uint8_t* lineptr = bbptr;
            for (int x = 0; x < vga->_frameWidth; x += 10) {
                // partially unrolled loop for speed
                pixelBits = *pptr;
                *(bbptr++) = pixelBits & 0b00000111;
                *(bbptr++) = pixelBits & 0b00000111;
                pixelBits >>= 4;
                *(bbptr++) = pixelBits & 0b00000111;
                *(bbptr++) = pixelBits & 0b00000111;
                pptr++;
                pixelBits = *pptr;
                *(bbptr++) = pixelBits & 0b00000111;
                *(bbptr++) = pixelBits & 0b00000111;
                pixelBits >>= 4;
                *(bbptr++) = pixelBits & 0b00000111;
                *(bbptr++) = pixelBits & 0b00000111;
                pptr++;
                pixelBits = *pptr;
                *(bbptr++) = pixelBits & 0b00000111;
                *(bbptr++) = pixelBits & 0b00000111;
                pixelBits >>= 4;
                *(bbptr++) = pixelBits & 0b00000111;
                *(bbptr++) = pixelBits & 0b00000111;
                pptr++;
                pixelBits = *pptr;
                *(bbptr++) = pixelBits & 0b00000111;
                *(bbptr++) = pixelBits & 0b00000111;
                pixelBits >>= 4;
                *(bbptr++) = pixelBits & 0b00000111;
                *(bbptr++) = pixelBits & 0b00000111;
                pptr++;
                pixelBits = *pptr;
                *(bbptr++) = pixelBits & 0b00000111;
                *(bbptr++) = pixelBits & 0b00000111;
                pixelBits >>= 4;
                *(bbptr++) = pixelBits & 0b00000111;
                *(bbptr++) = pixelBits & 0b00000111;
                pptr++;
            }
            memcpy(bbptr, lineptr, vga->_screenWidth);
            bbptr += vga->_screenWidth;
        }
    }

    if (pos_px >= vga->_screenWidth*(vga->_screenHeight-vga->_bounceBufferLines)) {
        if (xSemaphoreTakeFromISR(vga->_sem_gui_ready, NULL) == pdTRUE) {
            if (vga->_frameBufferIndex == 0) {
                vga->_frameBufferIndex = 1;
            } else {
                vga->_frameBufferIndex = 0;
            }
            xSemaphoreGiveFromISR(vga->_sem_vsync_end, NULL);
        }
    }

    return true;
}