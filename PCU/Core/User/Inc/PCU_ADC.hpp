#pragma once

#include <cstdint>
#include "PCU_Config.hpp"

// Indices of Channels
constexpr uint8_t BRAKE1 = 0; // Rank 1: IN1
constexpr uint8_t BRAKE2 = 1; // Rank 2: IN0
constexpr uint8_t APPS2 = 2;  // Rank 3: IN12
constexpr uint8_t APPS1 = 3;  // Rank 4: IN13

constexpr uint8_t ADC_NUM_CHANNELS = 4;

extern volatile uint16_t adc_buf[ADC_NUM_CHANNELS];

void PCU_ADC_Init();

class APPS_Sensor
{
public:
  APPS_Sensor(uint8_t apps_id, uint16_t min_raw, uint16_t max_raw);

  uint16_t getRaw();
  uint16_t get_torque();
  float get_percentage();
  bool in_range();

private:
  uint8_t apps_id;
  uint16_t min_raw;
  uint16_t max_raw;
};

extern APPS_Sensor apps1;
extern APPS_Sensor apps2;

bool validate_torque(APPS_Sensor &sensor1, APPS_Sensor &sensor2);

class Brake_Sensor
{
public:
  Brake_Sensor(uint8_t brake_id, uint16_t min_raw, uint16_t max_raw);

  uint16_t getRaw();
  float get_percentage();
  bool is_pressed();
  bool in_range();

private:
  uint8_t brake_id;
  uint16_t min_raw;
  uint16_t max_raw;
};

extern Brake_Sensor brake1;
extern Brake_Sensor brake2;

bool brakes_pressed(Brake_Sensor &sensor1, Brake_Sensor &sensor2);
