#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

void init_logger();
void logger_printf(const char * format, ...);

#endif // LOGGER_H
