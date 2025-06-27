#include <math.h>
#include <stdio.h>
#include "esp_lcd_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7735.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "image.h"  // Наш файл с изображением

// Распиновка 
#define PIN_LCD_CS      5
#define PIN_LCD_DC      17
#define PIN_LCD_RST     16
#define PIN_LCD_SCLK    18  // SCK для VSPI
#define PIN_LCD_MOSI    23  // MOSI для VSPI

// Размеры дисплея (128x160 для ST7735)
#define LCD_WIDTH       128
#define LCD_HEIGHT      160

static const char *TAG = "ST7735_Demo";

// Цвета в формате RGB565
#define COLOR_BLACK     0x0000
#define COLOR_BLUE      0x001F
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_YELLOW    0xFFE0
#define COLOR_WHITE     0xFFFF

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ST7735 initialization...");

    // 1. Настройка шины SPI (VSPI)
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2 + 8, // Буфер для всего экрана
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI bus initialized");

    // 2. Настройка интерфейса LCD
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = 20 * 1000 * 1000, // 40 MHz
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &io_handle));
    ESP_LOGI(TAG, "LCD IO interface created");

    // 3. Конфигурация панели
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR, // BGR порядок для ST7735
        .bits_per_pixel = 16,             // RGB565
        .flags = {
            .reset_active_high = 0        // Активный уровень сброса - LOW
        },
    };
    
    // Переопределение команд инициализации (Black Tab)
    const st7735_lcd_init_cmd_t custom_init_cmds[] = {
        {ST7735_SWRESET, (uint8_t[]){0x00}, 0, 150},
        {ST7735_SLPOUT, (uint8_t[]){0x00}, 0, 255},
        {ST7735_FRMCTR1, (uint8_t[]){0x01, 0x2C, 0x2D}, 3, 0},
        {ST7735_FRMCTR2, (uint8_t[]){0x01, 0x2C, 0x2D}, 3, 0},
        {ST7735_FRMCTR3, (uint8_t[]){0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}, 6, 0},
        {ST7735_INVCTR, (uint8_t[]){0x07}, 1, 0},
        {ST7735_PWCTR1, (uint8_t[]){0xA2, 0x02, 0x84}, 3, 0},
        {ST7735_PWCTR2, (uint8_t[]){0xC5}, 1, 0},
        {ST7735_PWCTR3, (uint8_t[]){0x0A, 0x00}, 2, 0},
        {ST7735_PWCTR4, (uint8_t[]){0x8A, 0x2A}, 2, 0},
        {ST7735_PWCTR5, (uint8_t[]){0x8A, 0xEE}, 2, 0},
        {ST7735_VMCTR1, (uint8_t[]){0x0E}, 1, 0},
        {ST7735_INVOFF, (uint8_t[]){0x00}, 0, 0},
        {ST7735_MADCTL, (uint8_t[]){0xC0}, 1, 0},//0xC8}, 1, 0}, // порядок пикселей и цветов
        {ST7735_COLMOD, (uint8_t[]){0x05}, 1, 0},
        {ST7735_GMCTRP1, (uint8_t[]){0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10}, 16, 0},
        {ST7735_GMCTRN1, (uint8_t[]){0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10}, 16, 0},
        {ST7735_NORON, (uint8_t[]){0x00}, 0, 10},
        {ST7735_DISPON, (uint8_t[]){0x00}, 0, 100},
    };
    //55
    st7735_vendor_config_t vendor_cfg = {
        .init_cmds = custom_init_cmds,
        .init_cmds_size = sizeof(custom_init_cmds) / sizeof(st7735_lcd_init_cmd_t)
    };
    panel_config.vendor_config = &vendor_cfg;

    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7735(io_handle, &panel_config, &panel_handle));
    ESP_LOGI(TAG, "ST7735 panel created");
// 4. Инициализация дисплея
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true)); // Ориентация как в Arduino
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_LOGI(TAG, "Display initialized");

    // 5. Демонстрация работы
    ESP_LOGI(TAG, "Starting display demo...");

    while(1){
    // Заливка экрана разными цветами
    const uint16_t colors[] = {
        COLOR_BLACK,
        0xF800,
        0x07E0,
        0x001F, //RGB
    };
    
    for (int i = 0; i < sizeof(colors)/sizeof(colors[0]); i++) {
        ESP_LOGI(TAG, "Filling with color: 0x%04X", colors[i]);
        
        // Создаем буфер для заливки (одна строка)
        uint16_t *line_buffer = malloc(LCD_WIDTH * sizeof(uint16_t));
        if (!line_buffer) {
            ESP_LOGE(TAG, "Failed to allocate line buffer!");
            continue;
        }
        
        // Заполняем буфер цветом
        for (int x = 0; x < LCD_WIDTH; x++) {
            line_buffer[x] = colors[i];
        }
        
        // Заливаем экран построчно
        for (int y = 0; y < LCD_HEIGHT; y++) {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_WIDTH, y+1, line_buffer);
        }
        
        free(line_buffer);
        vTaskDelay(pdMS_TO_TICKS(3000));
        }
        }
        
/*
    // Рисуем геометрические фигуры
    ESP_LOGI(TAG, "Drawing geometric shapes...");
    
    // Прямоугольник
    uint16_t rect_buffer[20 * 30];
    for (int y = 0; y < 30; y++) {
        for (int x = 0; x < 20; x++) {
            rect_buffer[y * 20 + x] = COLOR_CYAN;
        }
    }
    esp_lcd_panel_draw_bitmap(panel_handle, 10, 10, 30, 40, rect_buffer);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Линия
    uint16_t line_buffer[100];
    for (int i = 0; i < 100; i++) line_buffer[i] = COLOR_MAGENTA;
    esp_lcd_panel_draw_bitmap(panel_handle, 50, 70, 150, 71, line_buffer);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Круг (примитивная реализация)
    for (int r = 0; r < 20; r++) {
        uint16_t circle_buffer[3 * 3];
        for (int i = 0; i < 9; i++) circle_buffer[i] = COLOR_YELLOW;
        
        int x = 80 + r * cos(r * 0.3);
        int y = 100 + r * sin(r * 0.3);
        esp_lcd_panel_draw_bitmap(panel_handle, x, y, x+3, y+3, circle_buffer);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
*/
/*
    ESP_LOGI(TAG, "Display demo completed!");
    // 5. Вывод изображения
    ESP_LOGI(TAG, "Displaying image...");
    
    // Вариант 1: Прямой вывод (требует достаточно памяти)
     esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, image_data);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Вариант 2: Построчный вывод (более экономичный)
    uint16_t line_buffer[LCD_WIDTH];
    for (int y = 0; y < LCD_HEIGHT; y++) {
        // Копируем одну строку
        for (int x = 0; x < LCD_WIDTH; x++) {
            line_buffer[x] = image_data[y * LCD_WIDTH + x];
        }
        
        // Отправляем строку
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_WIDTH, y+1, line_buffer);
        
    }
    */
    ESP_LOGI(TAG, "Image displayed successfully!");


}