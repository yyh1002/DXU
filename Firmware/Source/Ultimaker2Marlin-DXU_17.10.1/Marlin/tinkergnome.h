#ifndef TINKERGNOME_H
#define TINKERGNOME_H

#include "temperature.h"
#include "UltiLCD2_menu_utils.h"

// low pass filter constant, from 0.0 to 1.0 -- Higher numbers mean more smoothing, less responsiveness.
// 0.0 would be completely disabled, 1.0 would ignore any changes
#define LOW_PASS_SMOOTHING 0.90
#define DEFAULT_FILAMENT_AREA 6.3793966

// allowed tuning difference to the target temperature
#define MAX_TEMP_DIFF  99
#define MAX_STANDBY_DIFF  50

extern float recover_height;
extern float recover_position[NUM_AXIS];
extern int recover_temperature[EXTRUDERS];

extern float e_smoothed_speed[EXTRUDERS];
extern float current_nominal_speed;

void tinkergnome_init();
void lcd_menu_maintenance_expert();
void lcd_menu_print_heatup_tg();
void lcd_menu_printing_tg();
void lcd_menu_move_axes();
#if FAN2_PIN != LED_PIN
void manage_led_timeout();
#endif
void manage_encoder_position(int8_t encoder_pos_interrupt);
void lcd_menu_expert_extrude();
void recover_start_print(const char *cmd);
void lcd_menu_recover_init();
void lcd_menu_expert_recover();
void reset_printing_state();
void endofprint_retract_store();
void lcd_prepare_buildplate_adjust();
void lcd_menu_simple_buildplate_init();


FORCE_INLINE void lcd_print_tune_nozzle0_diff() { lcd_tune_value(target_temperature_diff[0], max(-MAX_TEMP_DIFF, -target_temperature[0]), min(MAX_TEMP_DIFF, get_maxtemp(0) - target_temperature[0] - 15)); }
#if EXTRUDERS > 1
FORCE_INLINE void lcd_print_tune_nozzle1_diff() { lcd_tune_value(target_temperature_diff[1], max(-MAX_TEMP_DIFF, -target_temperature[1]), min(MAX_TEMP_DIFF, get_maxtemp(1) - target_temperature[1] - 15)); }
#endif
#if TEMP_SENSOR_BED != 0
FORCE_INLINE void lcd_print_tune_bed_diff() { lcd_tune_value(target_temperature_bed_diff, max(-MAX_TEMP_DIFF, -target_temperature_bed), min(MAX_TEMP_DIFF, BED_MAXTEMP - target_temperature_bed - 15)); }
#endif
#endif //TINKERGNOME_H
