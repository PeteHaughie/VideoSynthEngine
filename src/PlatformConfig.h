#pragma once

#if defined(__linux__) && __has_include(<wiringPi.h>)
    #define VSE_HAS_GPIO
    #include <wiringPi.h>
#endif
