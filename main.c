#include <stdint.h>
#include <stm32f10x.h>

// Антидребезг
void delay_debounce(void) {
    for (volatile uint32_t i = 0; i < 8000 * 50; i++) {
        __asm__("nop");
    }
}

int main(void) {
    // Включаем тактирование портов A, C и таймера TIM2
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    // Настройка PC13 как output (светодиод)
    GPIOC->CRH &= ~GPIO_CRH_CNF13;
    GPIOC->CRH |= GPIO_CRH_MODE13_1 | GPIO_CRH_MODE13_0;

    // Настройка PA0 и PA1 как input (кнопки)
    GPIOA->CRL &= ~(GPIO_CRL_CNF0 | GPIO_CRL_CNF1);
    GPIOA->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_MODE1);

    // Настраиваем TIM2
    TIM2->PSC = 3599;
    TIM2->ARR = 9999;
    TIM2->CR1 |= TIM_CR1_CEN; // запуск таймера

    while (1) {
        // Кнопка PA0: увеличить частоту → уменьшить PSC
        if (!(GPIOA->IDR & GPIO_IDR_IDR0)) {
            if (TIM2->PSC > 0) {
                TIM2->PSC = TIM2->PSC >> 1;
                TIM2->EGR = TIM_EGR_UG; // сгенерировать обновление, чтобы новый PSC вступил в силу
            }
            delay_debounce();
            while (!(GPIOA->IDR & GPIO_IDR_IDR0)); // ждём отпускания
        }

        // Кнопка PA1: уменьшить частоту → увеличить PSC
        if (!(GPIOA->IDR & GPIO_IDR_IDR1)) {
            if (TIM2->PSC < 0xFFFF) {
                TIM2->PSC = TIM2->PSC << 1;
                TIM2->EGR = TIM_EGR_UG; // применить новый PSC
            }
            delay_debounce();
            while (!(GPIOA->IDR & GPIO_IDR_IDR1)); // ждём отпускания
        }

        // Если произошло переполнение таймера — мигаем светодиодом
        if (TIM2->SR & TIM_SR_UIF) {
            TIM2->SR &= ~TIM_SR_UIF; // сброс флага
            GPIOC->ODR ^= GPIO_ODR_ODR13; // переключить PC13
        }
    }
}