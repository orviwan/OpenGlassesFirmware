#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

void initialize_logger();
void logger_printf(const char * format, ...);
void log_message(const char * format, ...);

#endif // LOGGER_H
