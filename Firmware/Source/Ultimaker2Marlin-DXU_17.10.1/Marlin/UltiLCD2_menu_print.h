#ifndef ULTI_LCD2_MENU_PRINT_H
#define ULTI_LCD2_MENU_PRINT_H

#include "cardreader.h"

#define LCD_CACHE_COUNT 6
#define LCD_CACHE_TEXT_SIZE_SHORT  20
#define LCD_CACHE_TEXT_SIZE_FULL   (LONG_FILENAME_LENGTH-1)
#define LCD_CACHE_TEXT_SIZE_REMAIN (LCD_CACHE_TEXT_SIZE_FULL - LCD_CACHE_TEXT_SIZE_SHORT)
#define LCD_CACHE_FILE_SIZE ((2 + LCD_CACHE_TEXT_SIZE_SHORT) * LCD_CACHE_COUNT + 1)
#define LCD_DETAIL_CACHE_SIZE (5 + 8*EXTRUDERS + 8*EXTRUDERS)
//
#define LCD_CACHE_ID(n) lcd_cache[(n)]
#define LCD_CACHE_TYPE(n) lcd_cache[LCD_CACHE_COUNT + (n)]
#define LCD_CACHE_FILENAME(n) ((char*)&lcd_cache[2*LCD_CACHE_COUNT + (n) * LCD_CACHE_TEXT_SIZE_SHORT])
#define LCD_CACHE_NR_OF_FILES() lcd_cache[(LCD_CACHE_COUNT*(LCD_CACHE_TEXT_SIZE_SHORT+2))]
//
#define LCD_CACHE_REMAIN_COUNT 1
#define LCD_CACHE_REMAIN_START (LCD_CACHE_FILE_SIZE + LCD_DETAIL_CACHE_SIZE)
#define LCD_CACHE_REMAIN_SIZE ((1 + LCD_CACHE_TEXT_SIZE_REMAIN) * LCD_CACHE_REMAIN_COUNT)
#define LCD_CACHE_REMAIN_ID(n) lcd_cache[LCD_CACHE_REMAIN_START + (n)]
#define LCD_CACHE_REMAIN_FILENAME(n) ((char*)&lcd_cache[LCD_CACHE_REMAIN_START + LCD_CACHE_REMAIN_COUNT + (n) * LCD_CACHE_TEXT_SIZE_REMAIN])
//
#define LCD_DETAIL_CACHE_START (LCD_CACHE_FILE_SIZE)
#define LCD_DETAIL_CACHE_ID() lcd_cache[LCD_DETAIL_CACHE_START]
#define LCD_DETAIL_CACHE_TIME() (*(uint32_t*)&lcd_cache[LCD_DETAIL_CACHE_START+1])
#define LCD_DETAIL_CACHE_MATERIAL(n) (*(uint32_t*)&lcd_cache[LCD_DETAIL_CACHE_START+5+4*n])
#define LCD_DETAIL_CACHE_NOZZLE_DIAMETER(n) (*(float*)&lcd_cache[LCD_DETAIL_CACHE_START+5+4*EXTRUDERS+4*n])
#define LCD_DETAIL_CACHE_MATERIAL_TYPE(n) ((char*)&lcd_cache[LCD_DETAIL_CACHE_START+5+8*EXTRUDERS+8*n])
//
#define LCD_CACHE_SIZE (LCD_CACHE_FILE_SIZE + LCD_DETAIL_CACHE_SIZE + LCD_CACHE_REMAIN_SIZE)
extern uint8_t lcd_cache[LCD_CACHE_SIZE];

// Use the lcd_cache memory to store manual moving positions
#define TARGET_POS(n)   (*(float*)&lcd_cache[(n) * sizeof(float)])
#define TARGET_MIN(n)   (*(float*)&lcd_cache[(n) * sizeof(float)])
#define TARGET_MAX(n)   (*(float*)&lcd_cache[sizeof(min_pos) + (n) * sizeof(float)])
#define OLD_FEEDRATE    (*(float*)&lcd_cache[NUM_AXIS * sizeof(float)])
#define OLD_ACCEL       (*(float*)&lcd_cache[(NUM_AXIS+1) * sizeof(float)])
#define OLD_JERK        (*(float*)&lcd_cache[(NUM_AXIS+2) * sizeof(float)])

extern unsigned long predictedTime;

extern uint8_t primed;
#define EXTRUDER_PRIMED      1
#define ENDOFPRINT_RETRACT 128

void lcd_menu_print_select();
void lcd_clear_cache();

void abortPrint(bool bQuickstop);

void lcd_print_pause();
void lcd_print_tune();
void lcd_print_abort();

void lcd_menu_print_abort();
void lcd_menu_print_tune();
void lcd_menu_print_ready();
void doStartPrint();
void lcd_change_to_menu_change_material_return();
void lcd_menu_print_pause();
void lcd_menu_print_resume();
void lcd_menu_print_heatup();

#endif//ULTI_LCD2_MENU_PRINT_H
