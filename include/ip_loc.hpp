#pragma once
#if __has_include(<Arduino.h>)
#include <Arduino.h>
#else
#include <esp_idf_version.h>
#include <stddef.h>
#endif
#ifndef ESP_PLATFORM
#error "This library only supports the ESP32 MCU."
#endif
#ifdef ARDUINO
namespace arduino {
#else
namespace esp_idf {
#endif
struct ip_loc final {    
    static bool fetch(float* out_lat,
                    float* out_lon, 
                    long* out_utc_offset, 
                    char* out_region, 
                    size_t region_size, 
                    char* out_city, 
                    size_t city_size,
                    char* out_time_zone,
                    size_t time_zone_size);
};
}