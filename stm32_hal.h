#ifndef __STM32_HAL__
#define __STM32_HAL__

// Select proper HAL based on MCU preprocessor definition
// MCU 모델에 따라 적절한 HAL 헤더를 선택

// ===================== F4 =====================
#if defined(STM32F401xx) || defined(STM32F405xx) || defined(STM32F407xx) || \
    defined(STM32F410xx) || defined(STM32F411xx) || defined(STM32F412xx) || \
    defined(STM32F413xx) || defined(STM32F415xx) || defined(STM32F417xx) || \
    defined(STM32F423xx) || defined(STM32F427xx) || defined(STM32F429xx) || \
    defined(STM32F437xx) || defined(STM32F439xx) || defined(STM32F446xx) || \
    defined(STM32F469xx) || defined(STM32F479xx)

    #include "stm32f4xx_hal.h"

// ===================== F7 =====================
#elif defined(STM32F722xx) || defined(STM32F723xx) || defined(STM32F730xx) || \
      defined(STM32F732xx) || defined(STM32F733xx) || defined(STM32F745xx) || \
      defined(STM32F746xx) || defined(STM32F750xx) || defined(STM32F756xx) || \
      defined(STM32F765xx) || defined(STM32F767xx) || defined(STM32F769xx) || \
      defined(STM32F777xx) || defined(STM32F779xx)

    #include "stm32f7xx_hal.h"

// ===================== L4 =====================
#elif defined(STM32L401xx) || defined(STM32L411xx) || defined(STM32L412xx) || \
      defined(STM32L422xx) || defined(STM32L431xx) || defined(STM32L432xx) || \
      defined(STM32L433xx) || defined(STM32L443xx) || defined(STM32L451xx) || \
      defined(STM32L452xx) || defined(STM32L462xx) || defined(STM32L471xx) || \
      defined(STM32L475xx) || defined(STM32L476xx) || defined(STM32L485xx) || \
      defined(STM32L486xx)

    #include "stm32l4xx_hal.h"

// ===================== WB =====================
#elif defined(STM32WB10xx) || defined(STM32WB15xx) || defined(STM32WB30xx) || \
      defined(STM32WB35xx) || defined(STM32WB50xx) || defined(STM32WB55xx)

    #include "stm32wbxx_hal.h"

// ===================== G4 =====================
#elif defined(STM32G431xx) || defined(STM32G441xx) || defined(STM32G471xx) || \
      defined(STM32G473xx) || defined(STM32G474xx) || defined(STM32G483xx) || \
      defined(STM32G484xx)

    #include "stm32g4xx_hal.h"

#else
    #error "Unsupported STM32 MCU"
#endif

#endif // __STM32_HAL__