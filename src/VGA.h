#pragma once
#include <esp_lcd_panel_rgb.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

class VGA {
public:
	VGA() {}
	~VGA() {}

	bool initWithSize(int frameWidth, int frameHeight, int bits);
	bool init(int width, int height, int scale = 2, int hborder = 0, int yborder = 0, int bits = 8, int* pins = NULL, bool usePsram = false);
	bool deinit();

	void vsyncWait();
	uint8_t* getDrawBuffer();

	int frameWidth() {
		return _frameWidth;
	}

	int frameHeight() {
		return _frameHeight;
	}

	int colorBits() {
		return _colorBits;
	}

protected:
	SemaphoreHandle_t _sem_vsync_end;
	SemaphoreHandle_t _sem_gui_ready;
	esp_lcd_panel_handle_t _panel_handle = NULL;
	uint8_t *_frameBuffers[2];
	int _frameBufferIndex = 0;
	int _frameWidth = 0;
	int _frameHeight = 0;
	int _screenWidth = 0;
	int _screenHeight = 0;
	int _frameScale = 2;
	int _colorBits = 8;
	int _bounceBufferLines = 0;
	int _hBorder = 0;
	int _vBorder = 0;
	int _lastBounceBufferPos = 0;
	bool _frameBuffersInPsram = false;

	bool validConfig(int width, int height, int scale = 2, int hborder = 0, int yborder = 0, int bits = 8, int* pins = NULL, bool usePsram = false);
	static bool vsyncEvent(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx);
	static bool bounceEvent(esp_lcd_panel_handle_t panel, void* bounce_buf, int pos_px, int len_bytes, void* user_ctx);
	void swapBuffers();
};