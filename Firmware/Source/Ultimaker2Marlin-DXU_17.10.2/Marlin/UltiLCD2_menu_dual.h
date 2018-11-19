#ifndef ULTILCD2_MENU_DUAL_H
#define ULTILCD2_MENU_DUAL_H

#include "Configuration.h"

#if EXTRUDERS > 1
#include "UltiLCD2_menu_utils.h"

void lcd_menu_dual();
void lcd_select_nozzle(menuFunc_t callbackOnSelect, menuFunc_t callbackOnAbort);
void switch_extruder(uint8_t newExtruder, bool moveZ);
void lcd_menu_tune_tcretract();
void lcd_init_extruderoffset();
void lcd_calc_extruderoffset();
void lcd_menu_extruderoffset();
void lcd_menu_fancontrol();
#endif // EXTRUDERS

#endif //ULTILCD2_MENU_DUAL_H
