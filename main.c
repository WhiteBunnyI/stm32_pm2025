#include <stdint.h>
#include <stm32f10x.h>

// Простая задержка
void delay_ms(uint32_t ms) {
	for (uint32_t i = 0; i < ms * 8000; i++){
		__asm__("nop");
	}
}


// Задержка с антидребезгом
void delay_debounce(void){
	delay_ms(50);
}

int main(void){
	// Включаем трактирование портов A и C
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;

	// Настройка PC13 как output (Светодиод)
	GPIOC->CRH &= ~GPIO_CRH_CNF13;
	GPIOC->CRH |= GPIO_CRH_MODE13_1 | GPIO_CRH_MODE13_0; // Output 50MHz

	// Настройка PA1 и PA0 (кнопки) как Input
	GPIOA->CRL &= ~(GPIO_CRL_CNF0 | GPIO_CRL_CNF1);
	GPIOA->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_MODE1);

	uint32_t frequencies[] = {1000, 500, 250, 125, 62, 31, 16, 8}; // Задержки в мс
	uint8_t current_freq = 0; // Начальная частота
	uint32_t delay = frequencies[current_freq];

	while(1){
		// Проверяем кнопку А (вверх) - увеличеваем частоту
		if (!(GPIOA->IDR & GPIO_IDR_IDR0)){
			if (current_freq > 0){
				current_freq--;
				delay = frequencies[current_freq];

			}

			delay_debounce();
			while(!(GPIOA->IDR & GPIO_IDR_IDR0)); // Ждем отпускания
		}

		// Проверяем кнопку С (вниз) - уменьшаем частоту
		if (!(GPIOA->IDR & GPIO_IDR_IDR1)){
			if (current_freq < 7){
				current_freq++;
				delay = frequencies[current_freq];

			}

			delay_debounce();
			while(!(GPIOA->IDR & GPIO_IDR_IDR1)); // Ждем отпускания
		}

		// Переключаем светодиод
		GPIOC->ODR ^= GPIO_ODR_ODR13;
		delay_ms(delay);
	}
}
