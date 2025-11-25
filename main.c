#include <stdint.h>
#include <stm32f10x.h>

// Простая задержка (приблизительно 1 мс при 72 МГц)
void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 8000; i++) {
        __asm__("nop");
    }
}

// Инициализация SPI1 в режиме Master, Mode 0 (CPOL=0, CPHA=0)
void SPI1_Init(void) {
    // Включаем тактирование GPIOA и SPI1
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_SPI1EN;

    // PA5 (SCK) и PA7 (MOSI) → Alternate Function Push-Pull, 50 MHz
    GPIOA->CRL &= ~(0xFF << 20); // очищаем биты для PA5 (биты 23:20)
    GPIOA->CRL |= (0xB << 20);   // CNF=10 (AF), MODE=11 → 1011 = 0xB
    GPIOA->CRL &= ~(0xFF << 28); // очищаем биты для PA7 (биты 31:28)
    GPIOA->CRL |= (0xB << 28);

    // PA4 = CS, PA1 = DC — обычные выходы, Push-Pull, 50 MHz
    GPIOA->CRL &= ~(0xF << 16); // PA4
    GPIOA->CRL |= (0x3 << 16);  // MODE=11, CNF=00 → Output PP
    GPIOA->CRL &= ~(0xF << 4);  // PA1
    GPIOA->CRL |= (0x3 << 4);

    // PA3 = /RST — выход, push-pull
    GPIOA->CRL &= ~(0xF << 12); // очистить биты для PA3
    GPIOA->CRL |= (0x3 << 12);  // MODE=11 (50 MHz), CNF=00 → Output PP

    // Установить /RST в HIGH (неактивное состояние)
    GPIOA->ODR |= (1 << 3);

    // Устанавливаем CS=1 (неактивен), DC=1 (режим данных по умолчанию)
    GPIOA->ODR |= (1 << 4) | (1 << 1);

    // Настройка SPI1: Master, Mode 0, f = 72 MHz / 8 = 9 MHz
    SPI1->CR1 = SPI_CR1_MSTR |          // Master
                SPI_CR1_SSM |           // Software Slave Management
                SPI_CR1_SSI |           // Internal NSS = 1
                (2 << 3);               // BR[2:0] = 010 → делитель 8
    SPI1->CR1 |= SPI_CR1_SPE;           // Включить SPI
}

// Отправка байта по SPI1
void SPI1_Write(uint8_t data) {
    while (!(SPI1->SR & SPI_SR_TXE)); // Ждём, пока буфер передачи освободится
    SPI1->DR = data;                  // Запись данных
    while (SPI1->SR & SPI_SR_BSY);    // Ждём завершения передачи
}

// Отправка команды на дисплей
void SSD1306_SendCmd(uint8_t cmd) {
    GPIOA->ODR &= ~(1 << 4); // /CE = LOW (активен)
    GPIOA->ODR &= ~(1 << 1); // DC = LOW → команда
    SPI1_Write(cmd);
    GPIOA->ODR |= (1 << 4);  // /CE = HIGH
}

// Отправка данных на дисплей
void SSD1306_SendData(uint8_t data) {
    GPIOA->ODR &= ~(1 << 4); // /CE = LOW
    GPIOA->ODR |= (1 << 1);  // DC = HIGH → данные
    SPI1_Write(data);
    GPIOA->ODR |= (1 << 4);  // /CE = HIGH
}

// Инициализация дисплея SSD1306
void SSD1306_Init(void) {
    // Хардварный сброс
    GPIOA->ODR &= ~(1 << 3); // /RST = LOW
    delay_ms(10);
    GPIOA->ODR |= (1 << 3);  // /RST = HIGH
    delay_ms(10);

    SSD1306_SendCmd(0xAE); // Display OFF
    SSD1306_SendCmd(0xD5); // Set Display Clock Div
    SSD1306_SendCmd(0x80);
    SSD1306_SendCmd(0xA8); // Set Multiplex Ratio
    SSD1306_SendCmd(0x3F);
    SSD1306_SendCmd(0xD3); // Set Display Offset
    SSD1306_SendCmd(0x00);
    SSD1306_SendCmd(0x40); // Set Display Start Line
    SSD1306_SendCmd(0x8D); // Charge Pump
    SSD1306_SendCmd(0x14);
    SSD1306_SendCmd(0x20); // Memory Mode
    SSD1306_SendCmd(0x00); // Horizontal Addressing Mode
    SSD1306_SendCmd(0xA1); // Segment Re-map
    SSD1306_SendCmd(0xC8); // COM Output Scan Direction
    SSD1306_SendCmd(0xDA); // Set COM Pins
    SSD1306_SendCmd(0x12);
    SSD1306_SendCmd(0x81); // Set Contrast
    SSD1306_SendCmd(0xCF);
    SSD1306_SendCmd(0xD9); // Pre-charge Period
    SSD1306_SendCmd(0xF1);
    SSD1306_SendCmd(0xDB); // VCOM Detect
    SSD1306_SendCmd(0x40);
    SSD1306_SendCmd(0xA4); // Entire Display ON
    SSD1306_SendCmd(0xA6); // Normal Display
    SSD1306_SendCmd(0xAF); // Display ON
}

// Отправка всего буфера (128x64 → 1024 байта)
void SSD1306_UpdateDisplay(const uint8_t* buffer) {
    // Установка диапазона столбцов
    SSD1306_SendCmd(0x21); // Set Column Address
    SSD1306_SendCmd(0x00); // Start Column
    SSD1306_SendCmd(0x7F); // End Column (127)

    // Установка диапазона страниц
    SSD1306_SendCmd(0x22); // Set Page Address
    SSD1306_SendCmd(0x00); // Start Page
    SSD1306_SendCmd(0x07); // End Page (7)

    // Отправка данных
    for (int i = 0; i < 1024; i++) {
        SSD1306_SendData(buffer[i]);
    }
}

// Генерация шахматной доски (8x8 клеток → 16x8 "блоков" по 8 пикселей)
void SSD1306_DrawCheckerboard(void) {
    static uint8_t buffer[1024]; // 128 * 64 / 8 = 1024

    // Очистка буфера
    for (int i = 0; i < 1024; i++) {
        buffer[i] = 0x00;
    }

    // Заполнение шахматного узора
    for (uint8_t page = 0; page < 8; page++) {
        for (uint8_t col = 0; col < 128; col++) {
            uint8_t block_x = col / 8;   // 8 пикселей в столбце = 1 байт
            uint8_t block_y = page;      // каждая страница = 8 строк
            if ((block_x + block_y) % 2 == 0) {
                buffer[page * 128 + col] = 0xFF; // Закрашенный блок
            }
        }
    }

    SSD1306_UpdateDisplay(buffer);
}

// Основная функция
int main(void) {
    SPI1_Init();
    delay_ms(100);
    SSD1306_Init();
    delay_ms(100);
    SSD1306_DrawCheckerboard();

    while (1) {
        // Ничего не делаем — изображение уже отображено
    }
}