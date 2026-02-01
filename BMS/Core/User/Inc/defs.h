#ifndef INC_DEFS_H_
#define INC_DEFS_H_

// ********************************** Debug Macros ********************************

// Enable/disable debug prints by uncommenting the line below
#define DEBUG_ENABLED   0

#if DEBUG_ENABLED
    #include <stdio.h>
    
    // Debug print macros for different sections
    #define DEBUG_VOLTAGE_START()     printf("[DEBUG_VOLTAGE] >>> ")
    #define DEBUG_VOLTAGE_END()       printf("\r\n")
    
    #define DEBUG_TEMP_START()        printf("[DEBUG_TEMP] >>> ")
    #define DEBUG_TEMP_END()          printf("\r\n")
    
    // Combined macros for easy use
    #define DEBUG_VOLTAGE_PRINT(...)  do { DEBUG_VOLTAGE_START(); printf(__VA_ARGS__); DEBUG_VOLTAGE_END(); } while(0)
    #define DEBUG_TEMP_PRINT(...)     do { DEBUG_TEMP_START(); printf(__VA_ARGS__); DEBUG_TEMP_END(); } while(0)
    
#else
    // No-op when debug is disabled
    #define DEBUG_VOLTAGE_START()     do {} while(0)
    #define DEBUG_VOLTAGE_END()       do {} while(0)
    #define DEBUG_TEMP_START()        do {} while(0)
    #define DEBUG_TEMP_END()          do {} while(0)
    #define DEBUG_VOLTAGE_PRINT(...)  do {} while(0)
    #define DEBUG_TEMP_PRINT(...)     do {} while(0)
#endif

#endif /* INC_DEFS_H_ */
