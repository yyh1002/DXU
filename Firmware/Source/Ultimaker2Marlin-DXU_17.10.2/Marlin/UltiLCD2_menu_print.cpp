#include <avr/pgmspace.h>

#include "Configuration.h"
#ifdef ENABLE_ULTILCD2
#include "Marlin.h"
#include "language.h"
#include "cardreader.h"
#include "temperature.h"
#include "lifetime_stats.h"
#include "filament_sensor.h"
#include "UltiLCD2.h"
#include "UltiLCD2_hi_lib.h"
#include "UltiLCD2_menu_print.h"
#include "UltiLCD2_menu_material.h"
#include "UltiLCD2_menu_maintenance.h"
#include "UltiLCD2_menu_utils.h"
#include "UltiLCD2_menu_prefs.h"
#include "preferences.h"
#include "tinkergnome.h"
#include "commandbuffer.h"
#if (EXTRUDERS > 1)
#include "ConfigurationDual.h"
#include "UltiLCD2_menu_dual.h"
#endif

uint8_t lcd_cache[LCD_CACHE_SIZE];

unsigned long predictedTime = 0;
uint8_t primed = 0;

void lcd_menu_print_heatup();
static void lcd_menu_print_printing();
static void lcd_menu_print_error_sd();
static void lcd_menu_print_error_position();
static void lcd_menu_print_classic_warning();
static void lcd_menu_print_material_warning();
static void lcd_menu_print_tune_retraction();

void lcd_clear_cache()
{
    for(uint8_t n=0; n<LCD_CACHE_COUNT; ++n)
        LCD_CACHE_ID(n) = 0xFF;
    for(uint8_t n=0; n<LCD_CACHE_REMAIN_COUNT; ++n)
        LCD_CACHE_REMAIN_ID(n) = 0xFF;
    LCD_DETAIL_CACHE_ID() = 0;
    LCD_CACHE_NR_OF_FILES() = 0xFF;
}

void abortPrint(bool bQuickstop)
{
    clear_command_queue();
    postMenuCheck = 0;

    // we're not printing any more
    if (card.sdprinting() && !card.pause())
    {
        recover_height = current_position[Z_AXIS];
    }
    card.stopPrinting();

    if (bQuickstop)
    {
        quickStop();
    }
    else
    {
        st_synchronize();
    }

    plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], active_extruder, true);

    // reset defaults
    feedmultiply = 100;
    for(uint8_t e=0; e<EXTRUDERS; ++e)
    {
        extrudemultiply[e] = 100;
    }

    float minTemp = get_extrude_min_temp();
    set_extrude_min_temp(0.0f);

#if EXTRUDERS > 1
    if (!bQuickstop && active_extruder)
    {
        switch_extruder(0, true);
    }
#endif // EXTRUDERS

    // set up the end of print retraction
    if ((primed & ENDOFPRINT_RETRACT) && (primed & (EXTRUDER_PRIMED << active_extruder)))
    {
        // retract
#if EXTRUDERS > 1
        if (!TOOLCHANGE_RETRACTED(active_extruder))
        {
            // add tool change retraction
            float retractlen = toolchange_retractlen[active_extruder]/volume_to_filament_length[active_extruder];
            if (EXTRUDER_RETRACTED(active_extruder))
            {
		        retractlen -= retract_recover_length[active_extruder];
		        if (retractlen < 0)
                {
                    retractlen = 0.0f;
                }
            }
            SET_TOOLCHANGE_RETRACT(active_extruder);
            toolchange_recover_length[active_extruder] = retractlen;

            // perform end-of-print retract
            plan_set_e_position(retractlen, active_extruder, true);
            current_position[E_AXIS] = 0.0f;
            plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], toolchange_retractfeedrate[active_extruder]/60, active_extruder);
        }
#else
        // perform the end-of-print retraction at the standard retract speed
        float retractlen = end_of_print_retraction / volume_to_filament_length[active_extruder];
        if (EXTRUDER_RETRACTED(active_extruder))
        {
            retractlen -= retract_recover_length[active_extruder];
            if (retractlen < 0)
            {
                retractlen = 0.0f;
            }
        }

        plan_set_e_position(retractlen, active_extruder, true);
        current_position[E_AXIS] = 0.0f;
        plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], retract_feedrate/60, active_extruder);
        CLEAR_EXTRUDER_RETRACT(active_extruder);
        retract_recover_length[active_extruder] = 0.0f;
#endif
    }

    // no longer primed
    primed = 0;
    set_extrude_min_temp(minTemp);
    doCooldown();

/*#if EXTRUDERS > 1
    // move to a safe y position in dual mode
    CommandBuffer::move2SafeYPos();
#endif // EXTRUDERS*/
    if (current_position[Z_AXIS] > max_pos[Z_AXIS] - 30)
    {
        CommandBuffer::homeHead();
        CommandBuffer::homeBed();
    }
    else
    {
        CommandBuffer::homeAll();
    }

    // finish all moves
    cmd_synchronize();
    finishAndDisableSteppers();
    current_position[E_AXIS] = 0.0f;
    plan_set_e_position(current_position[E_AXIS], active_extruder, true);

    stoptime=millis();
    lifetime_stats_print_end();

    printing_state = PRINT_STATE_NORMAL;
#if FAN2_PIN != LED_PIN
    if (led_mode == LED_MODE_WHILE_PRINTING)
        analogWrite(LED_PIN, 0);
#endif

    // reset to defaults
    fanSpeedPercent = 100;
    for(uint8_t e=0; e<EXTRUDERS; ++e)
    {
        volume_to_filament_length[e] = 1.0;
    }
    axis_relative_state = 0;
}

static void checkPrintFinished()
{
    if ((printing_state != PRINT_STATE_RECOVER) && (printing_state != PRINT_STATE_START) && !card.sdprinting() && !commands_queued() && !blocks_queued())
    {
        // normal end of gcode file
        recover_height = 0.0f;
        sleep_state |= SLEEP_COOLING;
        menu.return_to_main(false);
        menu.add_menu(menu_t(lcd_menu_print_ready, MAIN_MENU_ITEM_POS(0)), false);
        printing_state = PRINT_STATE_END;
        abortPrint(false);
    }
    else if (position_error)
    {
        sleep_state &= ~SLEEP_LED_OFF;
        menu.replace_menu(menu_t(lcd_menu_print_error_position, MAIN_MENU_ITEM_POS(0)));
        abortPrint(true);
    }
    else if (card.errorCode())
    {
        sleep_state &= ~SLEEP_LED_OFF;
        menu.replace_menu(menu_t(lcd_menu_print_error_sd, MAIN_MENU_ITEM_POS(0)));
        abortPrint(true);
    }
}

void doStartPrint()
{
    if (printing_state == PRINT_STATE_RECOVER)
    {
        return;
    }

    float priming_z = min(PRIMING_HEIGHT + recover_height, max_pos[Z_AXIS]-PRIMING_HEIGHT);

    // zero the extruder position
    current_position[E_AXIS] = 0.0;
    plan_set_e_position(current_position[E_AXIS], active_extruder, true);
	primed = 0;
	position_error = false;

	// since we are going to prime the nozzle, forget about any G10/G11 retractions that happened at end of previous print
	reset_retractstate();
    for (uint8_t e=0; e<EXTRUDERS; ++e)
    {
        CLEAR_EXTRUDER_RETRACT(e);
        retract_recover_length[e] = 0.0f;
    }

    // clear temperature flags
    temperature_state = 0;

    for(int8_t e = EXTRUDERS-1; (e>=0) && (printing_state < PRINT_STATE_ABORT); --e)
    {
        if (!LCD_DETAIL_CACHE_MATERIAL(e))
        {
            // don't prime the extruder if it isn't used in the (Ulti)gcode
            // traditional gcode files typically won't have the Material lines at start, so we won't prime for those
            // Also, on dual/multi extrusion files, only prime the extruders that are used in the gcode-file.
            continue;
        }

        extruder_lastused[e] = millis();

        if (!primed)
        {
            // move to priming height
            char buffer[20] = {0};
            sprintf_P(buffer, PSTR("G1 Z%d F%d"), (int)priming_z, (int)homing_feedrate[Z_AXIS]);
            enquecommand(buffer);
            // finish z-move
            cmd_synchronize();
            st_synchronize();
			if (printing_state == PRINT_STATE_ABORT)
			{
				return;
			}
        }

	#if (EXTRUDERS > 1)
        if (active_extruder != e)
        {
            // switch active extruder
            switch_extruder(e, true);
        }
        else
        {
            // move to heatup pos
            CommandBuffer::move2heatup();
            cmd_synchronize();
            st_synchronize();

            // wait for nozzle heatup
            reheatNozzle(active_extruder);

            if ((printing_state < PRINT_STATE_ABORT) && IS_WIPE_ENABLED)
            {
                // execute prime and wipe script
                cmdBuffer.processWipe(printing_state);
            }
        }
        if (!IS_WIPE_ENABLED && (printing_state < PRINT_STATE_ABORT))
        {
            // undo the tool change retraction
            float length = CommandBuffer::preparePriming(e) + PRIMING_MM3;
            // perform additional priming
            current_position[E_AXIS] = 0.0;
            plan_set_e_position(-length, e, true);
            plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], (PRIMING_MM3_PER_SEC * volume_to_filament_length[e]), e);

            CLEAR_TOOLCHANGE_RETRACT(e);
            CLEAR_EXTRUDER_RETRACT(e);

            // retract
            process_command_P(PSTR("G10"));
        }
	#else
		// undo the end-of-print retraction
		plan_set_e_position((0.0 - end_of_print_retraction) / volume_to_filament_length[e], e, true);
		plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], END_OF_PRINT_RECOVERY_SPEED, e);
		// perform additional priming
		plan_set_e_position(-PRIMING_MM3, e, true);
		plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], (PRIMING_MM3_PER_SEC * volume_to_filament_length[e]), e);
	#endif

        // finish priming moves and reset e-position
        set_current_position(E_AXIS, 0.0f);
        plan_set_e_position(current_position[E_AXIS], e, true);

        // note that we have primed, so that we know to de-prime at the end
        primed |= (EXTRUDER_PRIMED << e);
        primed |= ENDOFPRINT_RETRACT;
	}

    if (printing_state == PRINT_STATE_START)
    {
        // move to the recover start position
        plan_set_e_position(recover_position[E_AXIS], active_extruder, true);
        plan_buffer_line(recover_position[X_AXIS], recover_position[Y_AXIS], recover_position[Z_AXIS], recover_position[E_AXIS], min(homing_feedrate[X_AXIS], homing_feedrate[Z_AXIS])/60, active_extruder);
        for(int8_t i=0; i < NUM_AXIS; ++i) {
            current_position[i] = recover_position[i];
        }
        // first recovering move
        enquecommand(LCD_CACHE_FILENAME(0));
    }

    printing_state = PRINT_STATE_NORMAL;

    postMenuCheck = checkPrintFinished;
    card.startFileprint();
    lifetime_stats_print_start();
    starttime = millis();
    stoptime = starttime;
    predictedTime = 0;
}

static void userStartPrint()
{
    menu.return_to_main();
    if (printing_state == PRINT_STATE_RECOVER)
    {
        menu.add_menu(menu_t(lcd_menu_recover_init, lcd_menu_expert_recover, NULL, MAIN_MENU_ITEM_POS(3)));
    }
    else
    {
        recover_height = 0.0f;
        menu.add_menu(menu_t((ui_mode & UI_MODE_EXPERT) ? lcd_menu_printing_tg : lcd_menu_print_printing, MAIN_MENU_ITEM_POS(1)));
        doStartPrint();
    }
}

FORCE_INLINE void cardUpdir()
{
    card.updir();
}

static void lcd_sd_menu_filename_callback(uint8_t nr, uint8_t offsetY, uint8_t flags)
{
    char buffer[LONG_FILENAME_LENGTH] = {0};
    memset(buffer, '\0', sizeof(buffer));
    if (nr == 0)
    {
        if (card.atRoot())
        {
            lcd_cpyreturn(buffer);
        }else{
            strcpy_P(buffer, PSTR("< BACK"));
        }
    }else{
        uint8_t idx;
        for(idx=0; idx<LCD_CACHE_COUNT; ++idx)
        {
            if (LCD_CACHE_ID(idx) == nr)
            {
                strncpy(buffer, LCD_CACHE_FILENAME(idx), LCD_CACHE_TEXT_SIZE_SHORT);
                break;
            }
        }
        if (buffer[0] == '\0')
        {
            card.getFilenameFromNr(buffer, nr - 1);
            idx = nr % LCD_CACHE_COUNT;
            LCD_CACHE_ID(idx) = nr;
            strncpy(LCD_CACHE_FILENAME(idx), buffer, LCD_CACHE_TEXT_SIZE_SHORT);
            if (strlen(buffer) < LCD_CACHE_TEXT_SIZE_SHORT)
                LCD_CACHE_FILENAME(idx)[LCD_CACHE_TEXT_SIZE_SHORT-1] = '\0';
            LCD_CACHE_TYPE(idx) = card.filenameIsDir() ? 1 : 0;
            if (card.errorCode() && card.sdInserted())
            {
                // On a read error reset the file position and try to keep going. (not pretty, but these read errors are annoying as hell)
                card.clearError();
                LCD_CACHE_ID(idx) = 0xFF;
                card.clearLongFilename();
            }
        }
        if (flags & MENU_SELECTED)
        { // full filename length is needed
            // check if filename is short enough
            if (buffer[LCD_CACHE_TEXT_SIZE_SHORT-1] == '\0')
                goto far_break;
            // load from cache
            for(idx=0; idx<LCD_CACHE_REMAIN_COUNT; ++idx)
            {
                if (LCD_CACHE_REMAIN_ID(idx) == nr)
                {
                    strncpy(buffer+LCD_CACHE_TEXT_SIZE_SHORT, LCD_CACHE_REMAIN_FILENAME(idx), LCD_CACHE_TEXT_SIZE_REMAIN);
                    goto far_break;
                }
            }
            // nothing in cache - load from card
            card.getFilenameFromNr(buffer, nr - 1);
            if (card.errorCode() && card.sdInserted())
            {
                // On a read error try to keep going with short file name. (not pretty, but these read errors are annoying as hell)
                card.clearError();
                card.clearLongFilename();
                goto far_break;
            }
            idx = nr % LCD_CACHE_REMAIN_COUNT;
            LCD_CACHE_REMAIN_ID(idx) = nr;
            strncpy(LCD_CACHE_REMAIN_FILENAME(idx), buffer+LCD_CACHE_TEXT_SIZE_SHORT, LCD_CACHE_TEXT_SIZE_REMAIN);

            far_break:;
        }
    }
    lcd_draw_scroll_entry(offsetY, buffer, flags);
}

void lcd_sd_menu_details_callback(uint8_t nr)
{
    if (nr == 0)
    {
        return;
    }
    for(uint8_t idx=0; idx<LCD_CACHE_COUNT; ++idx)
    {
        if (LCD_CACHE_ID(idx) == nr)
        {
            if (LCD_CACHE_TYPE(idx) == 1)
            {
                lcd_lib_draw_string_centerP(BOTTOM_MENU_YPOS, PSTR("Folder"));
            }else{
                char buffer[64];
                if (LCD_DETAIL_CACHE_ID() != nr)
                {
                    card.getfilename(nr - 1);
                    if (card.errorCode())
                    {
                        card.clearError();
                        return;
                    }
                    LCD_DETAIL_CACHE_ID() = nr;
                    LCD_DETAIL_CACHE_TIME() = 0;
                    for(uint8_t e=0; e<EXTRUDERS; e++)
                    {
                        LCD_DETAIL_CACHE_MATERIAL(e) = 0;
                        LCD_DETAIL_CACHE_NOZZLE_DIAMETER(e) = 0.4;
                        LCD_DETAIL_CACHE_MATERIAL_TYPE(e)[0] = '\0';
                    }
                    card.openFile(card.currentFileName(), true);
                    if (card.isFileOpen())
                    {
                        for(uint8_t n=0;n<16;n++)
                        {
                            card.fgets(buffer, sizeof(buffer));
                            buffer[sizeof(buffer)-1] = '\0';
                            while (strlen(buffer) > 0 && buffer[strlen(buffer)-1] < ' ') buffer[strlen(buffer)-1] = '\0';
                            if (strncmp_P(buffer, PSTR(";TIME:"), 6) == 0)
                                LCD_DETAIL_CACHE_TIME() = strtol(buffer + 6, 0, 0);
                            else if (strncmp_P(buffer, PSTR(";MATERIAL:"), 10) == 0)
                            {
                                LCD_DETAIL_CACHE_MATERIAL(0) = strtol(buffer + 10, 0, 10);
                            }
                            else if (strncmp_P(buffer, PSTR(";NOZZLE_DIAMETER:"), 17) == 0)
                                LCD_DETAIL_CACHE_NOZZLE_DIAMETER(0) = strtod(buffer + 17, NULL);
                            else if (strncmp_P(buffer, PSTR(";MTYPE:"), 7) == 0)
                            {
                                strncpy(LCD_DETAIL_CACHE_MATERIAL_TYPE(0), buffer + 7, 8);
                                LCD_DETAIL_CACHE_MATERIAL_TYPE(0)[7] = '\0';
                            }
#if EXTRUDERS > 1
                            else if (strncmp_P(buffer, PSTR(";MATERIAL2:"), 11) == 0)
                            {
                                LCD_DETAIL_CACHE_MATERIAL(1) = strtol(buffer + 11, 0, 10);
                            }
                            else if (strncmp_P(buffer, PSTR(";NOZZLE_DIAMETER2:"), 18) == 0)
                                LCD_DETAIL_CACHE_NOZZLE_DIAMETER(1) = strtod(buffer + 18, NULL);
                            else if (strncmp_P(buffer, PSTR(";MTYPE2:"), 8) == 0)
                            {
                                strncpy(LCD_DETAIL_CACHE_MATERIAL_TYPE(1), buffer + 8, 8);
                                LCD_DETAIL_CACHE_MATERIAL_TYPE(1)[7] = '\0';
                            }
#endif
                        }
                    }
                    if (card.errorCode())
                    {
                        //On a read error reset the file position and try to keep going. (not pretty, but these read errors are annoying as hell)
                        card.clearError();
                        LCD_DETAIL_CACHE_ID() = 0xFF;
                    }
                }

                if (LCD_DETAIL_CACHE_TIME() > 0)
                {
                    char* c = buffer;
                    if (led_glow_dir || !(LCD_DETAIL_CACHE_MATERIAL(0) || LCD_DETAIL_CACHE_MATERIAL(1)))
                    {
                        if ((led_glow < 63) || !(LCD_DETAIL_CACHE_MATERIAL(0) || LCD_DETAIL_CACHE_MATERIAL(1)))
                        {
                            strcpy_P(c, PSTR("Time ")); c += 5;
                            c = int_to_time_min(LCD_DETAIL_CACHE_TIME(), c);
                            if (LCD_DETAIL_CACHE_TIME() < 60)
                            {
                                    strcat_P(c, PSTR("min"));
                            }
                            else
                            {
                                    strcat_P(c, PSTR("h"));
                            }
                        }
                        else
                        {
    #if EXTRUDERS > 1
                            strcpy_P(c, PSTR("Mat. ")); c += 5;
    #else
                            strcpy_P(c, PSTR("Material ")); c += 9;
    #endif
                            float length = float(LCD_DETAIL_CACHE_MATERIAL(0)) / (M_PI * (material[0].diameter / 2.0) * (material[0].diameter / 2.0));
                            if (length < 10000)
                                c = float_to_string2(length / 1000.0, c, PSTR("m"));
                            else
                                c = int_to_string(length / 1000.0, c, PSTR("m"));
    #if EXTRUDERS > 1
                            if (LCD_DETAIL_CACHE_MATERIAL(1))
                            {
                                *c++ = '/';
                                length = float(LCD_DETAIL_CACHE_MATERIAL(1)) / (M_PI * (material[1].diameter / 2.0) * (material[1].diameter / 2.0));
                                if (length < 10000)
                                    c = float_to_string2(length / 1000.0, c, PSTR("m"));
                                else
                                    c = int_to_string(length / 1000.0, c, PSTR("m"));
                            }
    #endif
                        }
                    }
                    else
                    {
                        strcpy_P(c, PSTR("Nozzle ")); c += 7;
    #if EXTRUDERS > 1
                        c = float_to_string2(LCD_DETAIL_CACHE_NOZZLE_DIAMETER(0), c, PSTR("/"));
                        c = float_to_string2(LCD_DETAIL_CACHE_NOZZLE_DIAMETER(1), c, NULL);
    #else
                        c = float_to_string2(LCD_DETAIL_CACHE_NOZZLE_DIAMETER(0), c);
    #endif
                    }
                    lcd_lib_draw_string(3, BOTTOM_MENU_YPOS, buffer);
                }else{
                    lcd_lib_draw_stringP(3, BOTTOM_MENU_YPOS, PSTR("No info available"));
                }
            }
        }
    }
}

void lcd_menu_print_select()
{
    if (!card.sdInserted())
    {
        LED_GLOW
        lcd_lib_encoder_pos = MAIN_MENU_ITEM_POS(0);
        lcd_info_screen(NULL, lcd_change_to_previous_menu);
        lcd_lib_draw_string_centerP(15, PSTR("No SD-CARD!"));
        lcd_lib_draw_string_centerP(25, PSTR("Please insert card"));
        lcd_lib_update_screen();
        card.release();
        return;
    }
    if (!card.isOk())
    {
        lcd_info_screen(NULL, lcd_change_to_previous_menu);
        lcd_lib_draw_string_centerP(16, PSTR("Reading card..."));
        lcd_lib_update_screen();
        lcd_clear_cache();
        card.initsd();
        return;
    }

    if (LCD_CACHE_NR_OF_FILES() == 0xFF)
        LCD_CACHE_NR_OF_FILES() = card.getnrfilenames();
    if (card.errorCode())
    {
        LCD_CACHE_NR_OF_FILES() = 0xFF;
        return;
    }
    uint8_t nrOfFiles = LCD_CACHE_NR_OF_FILES();
    if (nrOfFiles == 0)
    {
        if (card.atRoot())
            lcd_info_screen(reset_printing_state, lcd_change_to_previous_menu, PSTR("OK"));
        else
            lcd_info_screen(lcd_menu_print_select, cardUpdir, PSTR("OK"));
        lcd_lib_draw_string_centerP(25, PSTR("No files found!"));
        lcd_lib_update_screen();
        lcd_clear_cache();
        return;
    }

    if (lcd_lib_button_pressed)
    {
        uint16_t selIndex = uint16_t(SELECTED_SCROLL_MENU_ITEM());
        if (selIndex == 0)
        {
            if (card.atRoot())
            {
                reset_printing_state();
                menu.return_to_previous();
            }else{
                lcd_clear_cache();
                lcd_lib_keyclick();
                card.updir();
            }
        }else{
            card.getfilename(selIndex - 1);
            if (!card.filenameIsDir())
            {
                //Start print
                sleep_state = 0x0;
//#if EXTRUDERS > 1
//                switch_extruder(0, false);
//#else
//                active_extruder = 0;
//#endif // EXTRUDERS
                card.openFile(card.currentFileName(), true);
                if (card.isFileOpen() && !commands_queued())
                {
#if FAN2_PIN != LED_PIN
                    if (led_mode == LED_MODE_WHILE_PRINTING || led_mode == LED_MODE_BLINK_ON_DONE)
                        analogWrite(LED_PIN, 255 * int(led_brightness_level) / 100);
#endif
                    if (!card.currentLongFileName()[0])
                        card.setLongFilename(card.currentFileName());
                    card.truncateLongFilename(20);

                    char buffer[64];
                    card.fgets(buffer, sizeof(buffer));
                    buffer[sizeof(buffer)-1] = '\0';
                    if (strncmp_P(buffer, PSTR(";FLAVOR:UltiGCode"), 17) != 0)
                    {
                        card.fgets(buffer, sizeof(buffer));
                        buffer[sizeof(buffer)-1] = '\0';
                    }
                    card.setIndex(0);

                    lcd_clearstatus();
                    menu.return_to_main();

                    //reset all printing parameters to defaults
                    axis_relative_state = 0;
                    fanSpeed = 0;
                    feedmultiply = 100;
                    current_nominal_speed = 0.0f;
                    fanSpeedPercent = 100;
                    target_temperature_bed_diff = 0;
                    control_flags &= ~FLAG_MANUAL_FAN2;

                    for(uint8_t e=0; e<EXTRUDERS; ++e)
                    {
                        volume_to_filament_length[e] = 1.0f;
                        extrudemultiply[e] = 100;
                        e_smoothed_speed[e] = 0.0f;
                        target_temperature_diff[e] = 0;
                    }

                    if (strncmp_P(buffer, PSTR(";FLAVOR:UltiGCode"), 17) == 0)
                    {
                        //New style GCode flavor without start/end code.
                        // Temperature settings, filament settings, fan settings, start and end-code are machine controlled.
#if TEMP_SENSOR_BED != 0
                        target_temperature_bed = 0;
#endif
                        fanSpeedPercent = 0;
                        for(uint8_t e=0; e<EXTRUDERS; ++e)
                        {
//                            SERIAL_ECHOPGM("MATERIAL_");
//                            SERIAL_ECHO(e+1);
//                            SERIAL_ECHOPGM(": ");
//                            SERIAL_ECHOLN(LCD_DETAIL_CACHE_MATERIAL(e));

                            target_temperature[e] = 0;
                            volume_to_filament_length[e] = 1.0f / (M_PI * (material[e].diameter / 2.0) * (material[e].diameter / 2.0));
                            extrudemultiply[e] = material[e].flow;

                            if (LCD_DETAIL_CACHE_MATERIAL(e) < 1)
                                continue;

#if TEMP_SENSOR_BED != 0
                            target_temperature_bed = max(target_temperature_bed, material[e].bed_temperature);
#endif
                            fanSpeedPercent = max(fanSpeedPercent, material[e].fan_speed);
                        }

                        if (printing_state == PRINT_STATE_RECOVER)
                        {
                            menu.add_menu(menu_t(lcd_menu_recover_init, lcd_menu_expert_recover, NULL, MAIN_MENU_ITEM_POS(3)));
                        }
                        else
                        {
                            recover_height = 0.0f;

                            if (ui_mode & UI_MODE_EXPERT)
                                menu.add_menu(menu_t(lcd_menu_print_heatup_tg));
                            else
                                menu.add_menu(menu_t(lcd_menu_print_heatup));

                            if (strcasecmp(material[0].name, LCD_DETAIL_CACHE_MATERIAL_TYPE(0)) != 0)
                            {
                                if (strlen(material[0].name) > 0 && strlen(LCD_DETAIL_CACHE_MATERIAL_TYPE(0)) > 0)
                                {
                                    menu.replace_menu(menu_t(lcd_menu_print_material_warning), false);
                                }
                            }

                            // move to heatup position
                            CommandBuffer::homeAll();
                            cmd_synchronize();
#if EXTRUDERS < 2
                            CommandBuffer::move2heatup();
#endif
                            // printing_state = PRINT_STATE_NORMAL;
#if TEMP_SENSOR_BED != 0
                            if (target_temperature_bed > 0)
                                printing_state = PRINT_STATE_HEATING_BED;
                            else
#endif
                                printing_state = PRINT_STATE_HEATING;
                        }
                    }
                    else
                    {
                        //Classic gcode file
                        menu.add_menu(menu_t(lcd_menu_print_classic_warning, MAIN_MENU_ITEM_POS(0)));
                    }
                }
            }
            else
            {
                lcd_lib_keyclick();
                lcd_clear_cache();
                card.chdir(card.currentFileName());
                SELECT_SCROLL_MENU_ITEM(0);
            }
            return;//Return so we do not continue after changing the directory or selecting a file. The nrOfFiles is invalid at this point.
        }
    }
    lcd_scroll_menu(PSTR("SD CARD"), nrOfFiles+1, lcd_sd_menu_filename_callback, lcd_sd_menu_details_callback);
    lcd_lib_update_screen();
}

void lcd_menu_print_heatup()
{
    lcd_question_screen(lcd_menu_print_tune, NULL, PSTR("TUNE"), lcd_menu_print_abort, NULL, PSTR("ABORT"));

#if TEMP_SENSOR_BED != 0
    if (current_temperature_bed > degTargetBed() - TEMP_WINDOW*2)
    {
#endif
        printing_state = PRINT_STATE_HEATING;
        for(int8_t e=EXTRUDERS-1; e>=0; --e)
        {
            if (LCD_DETAIL_CACHE_MATERIAL(e) < 1)
                continue;
            if (target_temperature[e] <= 0)
                target_temperature[e] = material[e].temperature[nozzleSizeToTemperatureIndex(LCD_DETAIL_CACHE_NOZZLE_DIAMETER(e))];
            // limit power consumption: pre-heat only one nozzle at the same time
            if (target_temperature[e] > 0)
                break;
        }
#if TEMP_SENSOR_BED != 0
        if (current_temperature_bed >= degTargetBed() - TEMP_WINDOW * 2 && !commands_queued() && !blocks_queued())
#else
        if (!commands_queued() && !blocks_queued())
#endif // TEMP_SENSOR_BED
        {
            bool ready = false;
            for(int8_t e=EXTRUDERS-1; e>=0; --e)
            {
                if ((target_temperature[e] > 0) && (current_temperature[e] >= degTargetHotend(e) - TEMP_WINDOW))
                {
                    // set target temperature for other used nozzles
                    for(int8_t e2=EXTRUDERS-1; e2>=0; --e2)
                    {
                        if ((LCD_DETAIL_CACHE_MATERIAL(e2) < 1) || (target_temperature[e2] > 0))
                            continue;
                        target_temperature[e2] = material[e2].temperature[nozzleSizeToTemperatureIndex(LCD_DETAIL_CACHE_NOZZLE_DIAMETER(e2))];
                    }
                    ready = true;
                    break;
                }
            }

            if (ready)
            {
                if (ui_mode & UI_MODE_EXPERT)
                {
                    menu.replace_menu(menu_t(lcd_menu_printing_tg, MAIN_MENU_ITEM_POS(1)), false);
                }else{
                    menu.replace_menu(menu_t(lcd_menu_print_printing), false);
                }
                doStartPrint();
            }
        }
#if TEMP_SENSOR_BED != 0
    }
#endif // TEMP_SENSOR_BED

    uint8_t progress = 125;
    for(int8_t e=EXTRUDERS-1; e>=0; --e)
    {
        if (((printing_state != PRINT_STATE_RECOVER) && (LCD_DETAIL_CACHE_MATERIAL(e) < 1)) || (target_temperature[e] < 1))
            continue;
        if (current_temperature[e] > 20)
            progress = min(progress, (current_temperature[e] - 20) * 125 / (degTargetHotend(e) - 20 - TEMP_WINDOW));
        else
            progress = 0;
    }
#if TEMP_SENSOR_BED != 0
    if (current_temperature_bed > 20)
        progress = min(progress, (current_temperature_bed - 20) * 125 / (degTargetBed() - 20 - TEMP_WINDOW));
    else if (degTargetBed() > current_temperature_bed - 20)
        progress = 0;
#endif

    if (progress < minProgress)
        progress = minProgress;
    else
        minProgress = progress;

    lcd_lib_draw_string_centerP(10, PSTR("Heating up..."));
    lcd_lib_draw_string_centerP(20, PSTR("Preparing to print:"));
    lcd_lib_draw_string_center(30, card.currentLongFileName());

    lcd_progressbar(progress);

    lcd_lib_update_screen();
}

void lcd_change_to_menu_change_material_return()
{
    // plan_set_e_position(current_position[E_AXIS]);
    setTargetHotend(material[active_extruder].temperature[nozzleSizeToTemperatureIndex(LCD_DETAIL_CACHE_NOZZLE_DIAMETER(active_extruder))], active_extruder);
    menu.return_to_previous(false);
}

static void lcd_menu_print_printing()
{
    if (card.pause())
    {
        menu.add_menu(menu_t(lcd_select_first_submenu, lcd_menu_print_resume, NULL, MAIN_MENU_ITEM_POS(0)), true);
        if (!checkFilamentSensor())
        {
            menu.add_menu(menu_t(lcd_menu_filament_outage, MAIN_MENU_ITEM_POS(0)));
        }
    }
    else
    {
        lcd_question_screen(lcd_menu_print_tune, NULL, PSTR("TUNE"), lcd_menu_print_pause, lcd_select_first_submenu, PSTR("PAUSE"));
        uint8_t progress = card.getFilePos() / ((card.getFileSize() + 123) / 124);
        char buffer[32] = {0};
        switch(printing_state)
        {
        default:
            lcd_lib_draw_string_centerP(20, PSTR("Printing:"));
            lcd_lib_draw_string_center(30, card.currentLongFileName());
            break;
        case PRINT_STATE_HEATING:
            lcd_lib_draw_string_centerP(20, PSTR("Heating"));
#if EXTRUDERS > 1
            int_to_string(int(degTargetHotend(1)), int_to_string(int(dsp_temperature[1]), int_to_string(int(degTargetHotend(0)), int_to_string(int(dsp_temperature[0]), buffer, PSTR("C/")), PSTR("C ")), PSTR("C/")), PSTR("C"));
#else
            int_to_string(int(degTargetHotend(0)), int_to_string(int(dsp_temperature[0]), buffer, PSTR("C/")), PSTR("C"));
#endif // EXTRUDERS
            lcd_lib_draw_string_center(30, buffer);
            break;
#if TEMP_SENSOR_BED != 0
        case PRINT_STATE_HEATING_BED:
            lcd_lib_draw_string_centerP(20, PSTR("Heating buildplate"));
            int_to_string(int(degTargetBed()), int_to_string(dsp_temperature_bed, buffer, PSTR("C/")), PSTR("C"));
            lcd_lib_draw_string_center(30, buffer);
            break;
#endif
        }
        float printTimeMs = (millis() - starttime);
        float printTimeSec = printTimeMs / 1000L;
        float totalTimeMs = float(printTimeMs) * float(card.getFileSize()) / float(card.getFilePos());
        static float totalTimeSmoothSec;
        totalTimeSmoothSec = (totalTimeSmoothSec * 999L + totalTimeMs / 1000L) / 1000L;
        if (isinf(totalTimeSmoothSec))
            totalTimeSmoothSec = totalTimeMs;

        if (LCD_DETAIL_CACHE_TIME() == 0 && printTimeSec < 60)
        {
            totalTimeSmoothSec = totalTimeMs / 1000;
            lcd_lib_draw_stringP(5, 10, PSTR("Time left unknown"));
        }else{
            unsigned long totalTimeSec;
            if (printTimeSec < LCD_DETAIL_CACHE_TIME() / 2)
            {
                float f = float(printTimeSec) / float(LCD_DETAIL_CACHE_TIME() / 2);
                if (f > 1.0)
                    f = 1.0;
                totalTimeSec = float(totalTimeSmoothSec) * f + float(LCD_DETAIL_CACHE_TIME()) * (1 - f);
            }else{
                totalTimeSec = totalTimeSmoothSec;
            }
            unsigned long timeLeftSec;
            if (printTimeSec > totalTimeSec)
                timeLeftSec = 1;
            else
                timeLeftSec = totalTimeSec - printTimeSec;
            int_to_time_string(timeLeftSec, buffer);
            lcd_lib_draw_stringP(5, 10, PSTR("Time left"));
            lcd_lib_draw_string(65, 10, buffer);
        }

        lcd_progressbar(progress);
        lcd_lib_update_screen();
    }
}

static void lcd_menu_print_error_sd()
{
    LED_GLOW_ERROR
    lcd_info_screen(NULL, lcd_return_to_main_menu, PSTR("RETURN TO MAIN"));

    lcd_lib_draw_string_centerP(10, PSTR("Error while"));
    lcd_lib_draw_string_centerP(20, PSTR("reading SD-card!"));
    lcd_lib_draw_string_centerP(30, PSTR("Go to:"));
    lcd_lib_draw_string_centerP(40, PSTR("ultimaker.com/ER08"));
    /*
    char buffer[12];
    strcpy_P(buffer, PSTR("Code:"));
    int_to_string(card.errorCode(), buffer+5);
    lcd_lib_draw_string_center(40, buffer);
    */

    lcd_lib_update_screen();
}

static void lcd_menu_print_error_position()
{
    LED_GLOW_ERROR
    lcd_info_screen(NULL, lcd_return_to_main_menu, PSTR("RETURN TO MAIN"));

    lcd_lib_draw_string_centerP(15, PSTR("ERROR:"));
    lcd_lib_draw_string_centerP(25, PSTR("Tried printing out"));
    lcd_lib_draw_string_centerP(35, PSTR("of printing area"));

    lcd_lib_update_screen();
}

static void lcd_menu_print_classic_warning()
{
    lcd_question_screen(NULL, userStartPrint, PSTR("CONTINUE"), lcd_menu_print_select, lcd_remove_menu, PSTR("CANCEL"));

    lcd_lib_draw_string_centerP(10, PSTR("This file will"));
    lcd_lib_draw_string_centerP(20, PSTR("override machine"));
    lcd_lib_draw_string_centerP(30, PSTR("setting with setting"));
    lcd_lib_draw_string_centerP(40, PSTR("from the slicer."));

    lcd_lib_update_screen();
}

static void lcd_cancel_material_warning()
{
    doCooldown();
    lcd_return_to_main_menu();
}

static void lcd_menu_print_material_warning()
{
    lcd_question_screen(NULL, lcd_change_to_previous_menu, PSTR("CONTINUE"), NULL, lcd_cancel_material_warning, PSTR("CANCEL"));

    lcd_lib_draw_string_centerP(10, PSTR("This file is created"));
    lcd_lib_draw_string_centerP(20, PSTR("for a different"));
    lcd_lib_draw_string_centerP(30, PSTR("material."));
    char buffer[MATERIAL_NAME_SIZE * 2 + 5];
    sprintf_P(buffer, PSTR("%s vs %s"), material[0].name, LCD_DETAIL_CACHE_MATERIAL_TYPE(0));
    lcd_lib_draw_string_center(40, buffer);

    lcd_lib_update_screen();
}

static void lcd_menu_doabort()
{
    LED_GLOW
    if (printing_state == PRINT_STATE_ABORT)
    {
        lcd_lib_clear();
        lcd_lib_draw_string_centerP(20, PSTR("Aborting..."));
		lcd_lib_draw_gfx(LCD_GFX_WIDTH/2 - 4, 32, standbyGfx);
        lcd_lib_update_screen();
    }
    else
    {
        menu.return_to_main(false);
        menu.add_menu(menu_t(lcd_menu_print_ready, MAIN_MENU_ITEM_POS(0)), false);
    }
}

static void set_abort_state()
{
    printing_state = PRINT_STATE_ABORT;
    postMenuCheck = NULL;
    sleep_state &= ~SLEEP_LED_OFF;
    // force end of print retraction
    if (IS_SD_PRINTING && !card.pause())
    {
        primed |= (EXTRUDER_PRIMED << active_extruder);
        primed |= ENDOFPRINT_RETRACT;
    }
//    menu.return_to_main();
}

void lcd_menu_print_abort()
{
    LED_GLOW
    lcd_question_screen(lcd_menu_doabort, set_abort_state, PSTR("YES"), NULL, lcd_change_to_previous_menu, PSTR("NO"));
    lcd_lib_draw_string_centerP(20, PSTR("Abort the print?"));
    lcd_lib_draw_gfx(LCD_GFX_WIDTH/2 - 4, 32, standbyGfx);

    lcd_lib_update_screen();
}

static void postPrintReady()
{
    sleep_state &= ~SLEEP_LED_OFF;
#if FAN2_PIN != LED_PIN
    if (led_mode == LED_MODE_BLINK_ON_DONE)
        analogWrite(LED_PIN, 0);
#endif
    menu.return_to_previous();
}

void lcd_menu_print_ready()
{
#if FAN2_PIN != LED_PIN
    if ((led_mode == LED_MODE_BLINK_ON_DONE) && !(sleep_state & SLEEP_LED_OFF))
        analogWrite(LED_PIN, (led_glow << 1) * int(led_brightness_level) / 100);
#endif
    LED_GLOW
    lcd_info_screen(NULL, postPrintReady, PSTR("BACK TO MENU"));

    lcd_lib_draw_hline(3, 124, 13);
    // lcd_lib_draw_string_left(5, card.longFilename);

    char buffer[32] = {0};
    unsigned long t=(stoptime-starttime)/1000;

    char *c = buffer;
    strcpy_P(c, PSTR("Time ")); c += 5;
    c = int_to_time_min(t, c);
    if (t < 60)
    {
            strcat_P(c, PSTR("min"));
    }
    else
    {
            strcat_P(c, PSTR("h"));
    }
    lcd_lib_draw_string_center(5, buffer);

    c = buffer;
    for(uint8_t e=0; e<EXTRUDERS; e++)
        c = int_to_string(dsp_temperature[e], c, PSTR("C "));
#if TEMP_SENSOR_BED != 0
    int_to_string(dsp_temperature_bed, c, PSTR("C"));
#endif
    lcd_lib_draw_string_center(26, buffer);

    if (sleep_state & SLEEP_COOLING)
    {
#if TEMP_SENSOR_BED != 0
        if ((current_temperature[0] > 60) || (current_temperature_bed > 40))
#else
        if (current_temperature[0] > 60)
#endif // TEMP_SENSOR_BED
        {
            lcd_lib_draw_string_centerP(16, PSTR("Printer cooling down"));

            int16_t progress = 124 - (current_temperature[0] - 60);
            if (progress < 0) progress = 0;
            if (progress > 124) progress = 124;

            if (progress < minProgress)
                progress = minProgress;
            else
                minProgress = progress;

            lcd_progressbar(progress);
        }
        else
        {
            sleep_state &= ~SLEEP_COOLING;
        }
    }
    else
    {
        if (!(sleep_state & SLEEP_LED_OFF))
        {
            LED_GLOW
        }
        lcd_lib_draw_string_center(16, card.currentLongFileName());
        lcd_lib_draw_string_centerP(40, PSTR("Print finished"));
    }
    lcd_lib_update_screen();
}

static void tune_item_callback(uint8_t nr, uint8_t offsetY, uint8_t flags)
{
    uint8_t index = 0;
    char buffer[32] = {0};
    if (index++ == nr)
        lcd_cpyreturn(buffer);
    else if (index++ == nr)
        strcpy_P(buffer, PSTR("Speed"));
    else if (index++ == nr)
#if EXTRUDERS > 1
        strcpy_P(buffer, PSTR("Print Temperature 1"));
#else
        strcpy_P(buffer, PSTR("Temperature"));
#endif
#if EXTRUDERS > 1
    else if (index++ == nr)
        strcpy_P(buffer, PSTR("Print Temperature 2"));
#endif
#if TEMP_SENSOR_BED != 0
    else if (index++ == nr)
        strcpy_P(buffer, PSTR("Buildplate Temp."));
#endif
    else if (index++ == nr)
        strcpy_P(buffer, PSTR("Fan speed"));
#if EXTRUDERS > 1 && defined(FAN2_PIN) && FAN2_PIN > -1
    else if (index++ == nr)
        strcpy_P(buffer, PSTR("Fan control"));
#endif
    else if (index++ == nr)
#if EXTRUDERS > 1
        strcpy_P(buffer, PSTR("Material flow 1"));
#else
        strcpy_P(buffer, PSTR("Material flow"));
#endif
#if EXTRUDERS > 1
    else if (index++ == nr)
        strcpy_P(buffer, PSTR("Material flow 2"));
#endif
    else if (index++ == nr)
        strcpy_P(buffer, PSTR("Retraction"));
#if EXTRUDERS > 1
    else if (index++ == nr)
        strcpy_P(buffer, PSTR("Toolchange retract 1"));
    else if (index++ == nr)
        strcpy_P(buffer, PSTR("Toolchange retract 2"));
    else if (index++ == nr)
        strcpy_P(buffer, PSTR("Extruder offset"));
#endif
#if FAN2_PIN != LED_PIN
    else if (index++ == nr)
        strcpy_P(buffer, PSTR("LED Brightness"));
#endif
    else if ((ui_mode & UI_MODE_EXPERT) && card.sdprinting() && card.pause() && (index++ == nr))
        strcpy_P(buffer, PSTR("Move material"));
    else if ((ui_mode & UI_MODE_EXPERT) && (index++ == nr))
        strcpy_P(buffer, PSTR("Sleep timer"));

    lcd_draw_scroll_entry(offsetY, buffer, flags);
}

static void tune_item_details_callback(uint8_t nr)
{
    char buffer[32] = {0};
    if (nr == 1)
        int_to_string(feedmultiply, buffer, PSTR("%"));
    else if (nr == 2)
    {
        int_to_string(degTargetHotend(0), int_to_string(dsp_temperature[0], buffer, PSTR("C/")), PSTR("C"));
    }
#if EXTRUDERS > 1
    else if (nr == 3)
    {
        int_to_string(degTargetHotend(1), int_to_string(dsp_temperature[1], buffer, PSTR("C/")), PSTR("C"));
    }
#endif
#if TEMP_SENSOR_BED != 0
    else if (nr == 2 + EXTRUDERS)
    {
        int_to_string(int(degTargetBed()), int_to_string(dsp_temperature_bed, buffer, PSTR("C/")), PSTR("C"));
    }
#endif
    else if (nr == 2 + BED_MENU_OFFSET + EXTRUDERS)
        int_to_string(int(fanSpeed) * 100 / 255, buffer, PSTR("%"));
#if EXTRUDERS > 1 && defined(FAN2_PIN) && FAN2_PIN > -1
    else if (nr == 3 + BED_MENU_OFFSET + EXTRUDERS)
        strcpy_P(buffer, (control_flags & FLAG_SEPARATE_FAN) ? PSTR("separate fan control") : PSTR("main fan always on"));
#endif
    else if (nr == 3 + FAN_MENU_OFFSET + BED_MENU_OFFSET + EXTRUDERS)
        int_to_string(extrudemultiply[0], buffer, PSTR("%"));
#if EXTRUDERS > 1
    else if (nr == 4 + FAN_MENU_OFFSET + BED_MENU_OFFSET + EXTRUDERS)
        int_to_string(extrudemultiply[1], buffer, PSTR("%"));
#endif
#if FAN2_PIN != LED_PIN
    else if (nr == 1 + FAN_MENU_OFFSET + BED_MENU_OFFSET + 5*EXTRUDERS)
    {
        int_to_string(led_brightness_level, buffer, PSTR("%"));
    }
#endif
    else
        return;
    lcd_lib_draw_string_left(BOTTOM_MENU_YPOS, buffer);
}

static void lcd_menu_print_tune_heatup_nozzle(uint8_t e, int16_t max_temp)
{
    lcd_tune_value(target_temperature_diff[e], max(-MAX_TEMP_DIFF, -target_temperature[e]), min(MAX_TEMP_DIFF, max_temp - target_temperature[e] - 15));

    if (lcd_lib_button_pressed)
    {
        menu.return_to_previous();
    }

    lcd_lib_clear();
    char buffer[24];
#if EXTRUDERS > 1
    lcd_lib_draw_string_centerP(10, PSTR("Temperature"));
    strcpy_P(buffer, PSTR("Nozzle "));
    int_to_string(e+1, buffer+7, 0);
    lcd_lib_draw_string_center(20, buffer);
#else
    lcd_lib_draw_string_centerP(20, PSTR("Nozzle temperature:"));
#endif
    lcd_lib_draw_string_centerP(53, PSTR("Click to return"));
    char * c = int_to_string(int(dsp_temperature[e]), buffer, PSTR("C/"));
    c = int_to_string(int(degTargetHotend(e)), c, PSTR("C"));
    if (target_temperature_diff[e])
    {
        // append relative difference
        int_to_string(target_temperature_diff[e], c, PSTR(")"), PSTR(" ("), true);
    }
    lcd_lib_draw_string_center(35, buffer);
    lcd_lib_draw_heater(e ? LCD_GFX_WIDTH - LCD_CHAR_MARGIN_RIGHT - LCD_CHAR_SPACING : LCD_CHAR_MARGIN_LEFT, 10, getHeaterPower(e));
    lcd_lib_update_screen();
}

#if TEMP_SENSOR_BED != 0
static void lcd_menu_print_tune_heatup_bed()
{
    lcd_tune_value(target_temperature_bed_diff, max(-MAX_TEMP_DIFF, -target_temperature_bed), min(MAX_TEMP_DIFF, BED_MAXTEMP - target_temperature_bed - 15));

    if (lcd_lib_button_pressed)
    {
        menu.return_to_previous();
    }

    lcd_lib_clear();
    char buffer[24];
    lcd_lib_draw_string_centerP(10, PSTR("Temperature"));
    lcd_lib_draw_string_centerP(20, PSTR("Buildplate"));
    lcd_lib_draw_string_centerP(53, PSTR("Click to return"));
    char * c = int_to_string(int(dsp_temperature_bed), buffer, PSTR("C/"));
    c = int_to_string(int(degTargetBed()), c, PSTR("C"));
    if (target_temperature_bed_diff)
    {
        // append relative difference
        int_to_string(target_temperature_bed_diff, c, PSTR(")"), PSTR(" ("), true);
    }
    lcd_lib_draw_string_center(35, buffer);
    lcd_lib_draw_heater(LCD_CHAR_MARGIN_LEFT, 10, getHeaterPower(-1));
    lcd_lib_update_screen();
}
#endif // TEMP_SENSOR_BED

static void lcd_menu_print_tune_heatup_nozzle0()
{
    lcd_menu_print_tune_heatup_nozzle(0, HEATER_0_MAXTEMP);
}

#if EXTRUDERS > 1
void lcd_menu_print_tune_heatup_nozzle1()
{
    lcd_menu_print_tune_heatup_nozzle(1, HEATER_1_MAXTEMP);
}
#endif

void lcd_menu_print_tune()
{
#if FAN2_PIN != LED_PIN
    uint8_t len = 2 + FAN_MENU_OFFSET + BED_MENU_OFFSET + EXTRUDERS * 5;
#else
    uint8_t len = 1 + FAN_MENU_OFFSET + BED_MENU_OFFSET + EXTRUDERS * 5;
#endif
    if (ui_mode & UI_MODE_EXPERT)
    {
        ++len; // sleep timer
        if (card.sdprinting() && card.pause())
        {
            ++len; // move material
        }
    }

    lcd_scroll_menu(PSTR("TUNE"), len, tune_item_callback, tune_item_details_callback);
    if (lcd_lib_button_pressed)
    {
        uint8_t index(0);
        if (IS_SELECTED_SCROLL(index++))
            menu.return_to_previous();
        else if (IS_SELECTED_SCROLL(index++))
            LCD_EDIT_SETTING(feedmultiply, "Print speed", "%", 10, 1000);
        else if (IS_SELECTED_SCROLL(index++))
            menu.add_menu(menu_t(lcd_menu_print_tune_heatup_nozzle0, 0));
#if EXTRUDERS > 1
        else if (IS_SELECTED_SCROLL(index++))
            menu.add_menu(menu_t(lcd_menu_print_tune_heatup_nozzle1, 0));
#endif
#if TEMP_SENSOR_BED != 0
        else if (IS_SELECTED_SCROLL(index++))
            menu.add_menu(menu_t(lcd_menu_print_tune_heatup_bed, 0));
#endif
        else if (IS_SELECTED_SCROLL(index++))
            LCD_EDIT_SETTING_BYTE_PERCENT(fanSpeed, "Fan speed", "%", 0, 100);
#if EXTRUDERS > 1 && defined(FAN2_PIN) && FAN2_PIN > -1
        else if (IS_SELECTED_SCROLL(index++))
            menu.add_menu(menu_t(lcd_menu_fancontrol, MAIN_MENU_ITEM_POS(1)));
#endif
#if EXTRUDERS > 1
        else if (IS_SELECTED_SCROLL(index++))
            LCD_EDIT_SETTING(extrudemultiply[0], "Material flow 1", "%", 10, 1000);
        else if (IS_SELECTED_SCROLL(index++))
            LCD_EDIT_SETTING(extrudemultiply[1], "Material flow 2", "%", 10, 1000);
#else
        else if (IS_SELECTED_SCROLL(index++))
            LCD_EDIT_SETTING(extrudemultiply[0], "Material flow", "%", 10, 1000);
#endif
        else if (IS_SELECTED_SCROLL(index++))
            menu.add_menu(menu_t(lcd_menu_print_tune_retraction, 0));
#if EXTRUDERS > 1
        else if (IS_SELECTED_SCROLL(index++))
        {
            menu_extruder = 0;
            menu.add_menu(menu_t(lcd_menu_tune_tcretract, MAIN_MENU_ITEM_POS(1)));
        }
        else if (IS_SELECTED_SCROLL(index++))
        {
            menu_extruder = 1;
            menu.add_menu(menu_t(lcd_menu_tune_tcretract, MAIN_MENU_ITEM_POS(1)));
        }
        else if (IS_SELECTED_SCROLL(index++))
            menu.add_menu(menu_t(lcd_init_extruderoffset, lcd_menu_extruderoffset, lcd_calc_extruderoffset, MAIN_MENU_ITEM_POS(1)));
#endif
#if FAN2_PIN != LED_PIN
        else if (IS_SELECTED_SCROLL(index++))
            LCD_EDIT_SETTING(led_brightness_level, "Brightness", "%", 0, 100);
#endif
        else if ((ui_mode & UI_MODE_EXPERT) && card.sdprinting() && card.pause() && IS_SELECTED_SCROLL(index++))
            menu.add_menu(menu_t(lcd_menu_expert_extrude, 0)); // Move material
        else if ((ui_mode & UI_MODE_EXPERT) && IS_SELECTED_SCROLL(index++))
            menu.add_menu(menu_t(lcd_menu_sleeptimer));
    }
    lcd_lib_update_screen();
}

static void lcd_retraction_item(uint8_t nr, uint8_t offsetY, uint8_t flags)
{
    char buffer[32] = {0};
    if (nr == 0)
        lcd_cpyreturn(buffer);
    else if (nr == 1)
        strcpy_P(buffer, PSTR("Retract length"));
    else if (nr == 2)
        strcpy_P(buffer, PSTR("Retract speed"));
    else if (nr == 3)
        strcpy_P(buffer, PSTR("End of print length"));
#if EXTRUDERS > 1
    else if (nr == 4)
        strcpy_P(buffer, PSTR("Extruder change len"));
#endif

    lcd_draw_scroll_entry(offsetY, buffer, flags);
}

static void lcd_retraction_details(uint8_t nr)
{
    char buffer[32] = {0};
    if(nr == 1)
        float_to_string2(retract_length, buffer, PSTR("mm"));
    else if(nr == 2)
        int_to_string(retract_feedrate / 60 + 0.5, buffer, PSTR("mm/sec"));
    else if(nr == 3)
        float_to_string2(end_of_print_retraction, buffer, PSTR("mm"));
    else
        return;
    lcd_lib_draw_string(5, BOTTOM_MENU_YPOS, buffer);
}

static void lcd_menu_print_tune_retraction()
{
    lcd_scroll_menu(PSTR("RETRACTION"), 4 + (EXTRUDERS > 1 ? 1 : 0), lcd_retraction_item, lcd_retraction_details);
    if (lcd_lib_button_pressed)
    {
        if (IS_SELECTED_SCROLL(0))
            menu.return_to_previous();
        else if (IS_SELECTED_SCROLL(1))
            LCD_EDIT_SETTING_FLOAT001(retract_length, "Retract length", "mm", 0, 50);
        else if (IS_SELECTED_SCROLL(2))
            LCD_EDIT_SETTING_SPEED(retract_feedrate, "Retract speed", "mm/sec", 0, max_feedrate[E_AXIS] * 60);
    }
    lcd_lib_update_screen();
}

void lcd_print_pause()
{
    if (!card.pause())
    {
        card.pauseSDPrint();

        // move z up according to the current height - but minimum to z=70mm (above the gantry height)
        uint16_t zdiff = 0;
        if (current_position[Z_AXIS] < 70)
            zdiff = max(70 - floor(current_position[Z_AXIS]), 20);
        else if (current_position[Z_AXIS] < max_pos[Z_AXIS] - 60)
        {
            zdiff = 20;
        }
        else if (current_position[Z_AXIS] < max_pos[Z_AXIS] - 30)
        {
            zdiff = 2;
        }

        char buffer[32] = {0};
        char buffer_len[10];
    #if (EXTRUDERS > 1)
        float_to_string2(toolchange_retractlen[active_extruder], buffer_len, NULL);
        uint16_t x = max(5, int(min_pos[X_AXIS]) + 5 + roundOffset(X_AXIS, extruder_offset[X_AXIS][active_extruder]));
        uint16_t y = IS_DUAL_ENABLED ? (int)min_pos[Y_AXIS]+60 : (int)min_pos[Y_AXIS]+5;
        sprintf_P(buffer, PSTR("M601 X%u Y%u Z%u L%s"), x, y, zdiff, buffer_len);
    #else
        float_to_string2(end_of_print_retraction, buffer_len, NULL);
        sprintf_P(buffer, PSTR("M601 X5 Y10 Z%u L%s"), zdiff, buffer_len);
    #endif
        process_command(buffer, false);
        // clear flag for end of print retraction
        primed &= ~ENDOFPRINT_RETRACT;
    }
}

void lcd_print_tune()
{
    // switch to tune menu
    menu.add_menu(menu_t(lcd_menu_print_tune));
}

void lcd_print_abort()
{
    // switch to abort menu
    menu.add_menu(menu_t(lcd_menu_print_abort, MAIN_MENU_ITEM_POS(1)));
}

static void lcd_print_resume()
{
    menu.return_to_previous();
    card.resumePrinting();
    primed |= (EXTRUDER_PRIMED << active_extruder);
    primed |= ENDOFPRINT_RETRACT;
    for (uint8_t e=0; e<EXTRUDERS; ++e)
    {
        check_preheat(e);
    }
}

static void lcd_print_change_material()
{
    if (!blocks_queued())
    {
        menu_extruder = active_extruder;
        lcd_material_change_init(true);
        menu.add_menu(menu_t(lcd_change_to_menu_change_material_return), false);
        menu.add_menu(menu_t(lcd_menu_change_material_preheat));
    }
}

static void lcd_show_pause_menu()
{
    menu.replace_menu(menu_t(lcd_select_first_submenu, lcd_menu_print_resume, NULL, MAIN_MENU_ITEM_POS(0)));
    lcd_print_pause();
}

static const menu_t & get_pause_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t menu_index = 0;
    if (nr == menu_index++)
    {
        opt.setData(MENU_NORMAL, lcd_show_pause_menu);
    }
    else if (IS_SD_PRINTING && (nr == menu_index++))
    {
        opt.setData(MENU_NORMAL, lcd_print_abort);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_NORMAL, lcd_print_tune);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_NORMAL, lcd_change_to_previous_menu);
    }
    return opt;
}

static void drawPauseSubmenu(uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    if (nr == index++)
    {
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT+3
                                , LCD_LINE_HEIGHT
                                , 52
                                , LCD_LINE_HEIGHT*4
                                , PSTR("PAUSE|")
                                , ALIGN_CENTER
                                , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_gfx(LCD_CHAR_MARGIN_LEFT+26, LCD_LINE_HEIGHT*3+2, pauseGfx);
        }
        else
        {
            lcd_lib_draw_gfx(LCD_CHAR_MARGIN_LEFT+26, LCD_LINE_HEIGHT*3+2, pauseGfx);
        }
    }
    else if (IS_SD_PRINTING && (nr == index++))
    {
        LCDMenu::drawMenuString_P(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT+3
                                , LCD_LINE_HEIGHT
                                , 52
                                , LCD_LINE_HEIGHT*4
                                , PSTR("ABORT|")
                                , ALIGN_CENTER
                                , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_gfx(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT+26, LCD_LINE_HEIGHT*3+2, standbyGfx);
        }
        else
        {
            lcd_lib_draw_gfx(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT+26, LCD_LINE_HEIGHT*3+2, standbyGfx);
        }
    }
    else if (nr == index++)
    {
        LCDMenu::drawMenuBox(LCD_CHAR_MARGIN_LEFT*2
                           , BOTTOM_MENU_YPOS
                           , 48
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_stringP(LCD_CHAR_MARGIN_LEFT + 3*LCD_CHAR_SPACING+1, BOTTOM_MENU_YPOS, PSTR("TUNE"));
            lcd_lib_clear_gfx(2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, menuGfx);
        }
        else
        {
            lcd_lib_draw_stringP(LCD_CHAR_MARGIN_LEFT + 3*LCD_CHAR_SPACING+1, BOTTOM_MENU_YPOS, PSTR("TUNE"));
            lcd_lib_draw_gfx(2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, menuGfx);
        }
    }
    else if (nr == index++)
    {
        LCDMenu::drawMenuBox(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT*2
                                , BOTTOM_MENU_YPOS
                                , 48
                                , LCD_CHAR_HEIGHT
                                , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_stringP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 3*LCD_CHAR_SPACING+1, BOTTOM_MENU_YPOS, PSTR("BACK"));
            lcd_lib_clear_gfx(LCD_GFX_WIDTH/2 + 2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, backGfx);
        }
        else
        {
            lcd_lib_draw_stringP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 3*LCD_CHAR_SPACING+1, BOTTOM_MENU_YPOS, PSTR("BACK"));
            lcd_lib_draw_gfx(LCD_GFX_WIDTH/2 + 2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, backGfx);
        }
    }
}

void lcd_menu_print_pause()
{
    lcd_lib_clear();
    lcd_lib_draw_vline(64, 5, 46);
    lcd_lib_draw_hline(3, 124, 50);

    uint8_t len = IS_SD_PRINTING ? 4 : 3;
    menu.process_submenu(get_pause_menuoption, len);

    for (uint8_t index=0; index<len; ++index)
    {
        menu.drawSubMenu(drawPauseSubmenu, index);
    }
    lcd_lib_update_screen();
}

static const menu_t & get_resume_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t menu_index = 0;
    if (nr == menu_index++)
    {
        opt.setData(MENU_NORMAL, lcd_print_resume);
    }
    else if (IS_SD_PRINTING && (nr == menu_index++))
    {
        opt.setData(MENU_NORMAL, lcd_print_change_material);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_NORMAL, lcd_print_tune);
    }
    else if (IS_SD_PRINTING && (nr == menu_index++))
    {
        opt.setData(MENU_NORMAL, lcd_print_abort);
    }
    return opt;
}

static void drawResumeSubmenu(uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    if (nr == index++)
    {
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT+3
                                , LCD_LINE_HEIGHT
                                , 52
                                , LCD_LINE_HEIGHT*4
                                , movesplanned() ? PSTR("PAUSING|") : PSTR("RESUME|")
                                , ALIGN_CENTER
                                , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_gfx(LCD_CHAR_MARGIN_LEFT+26, LCD_LINE_HEIGHT*3+2, startGfx);
        }
        else
        {
            lcd_lib_draw_gfx(LCD_CHAR_MARGIN_LEFT+26, LCD_LINE_HEIGHT*3+2, startGfx);
        }
    }
    else if (IS_SD_PRINTING && (nr == index++))
    {
        LCDMenu::drawMenuBox(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT+3
                           , LCD_LINE_HEIGHT
                           , 52
                           , LCD_LINE_HEIGHT*4
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_string_center_atP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT+29, LCD_LINE_HEIGHT+LCD_LINE_HEIGHT/2+2, PSTR("CHANGE"));
            lcd_lib_clear_string_center_atP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT+29, 2*LCD_LINE_HEIGHT+LCD_LINE_HEIGHT/2+2, PSTR("MATERIAL"));
            lcd_lib_clear_gfx(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT+26, 3*LCD_LINE_HEIGHT+LCD_LINE_HEIGHT/2+2, filamentGfx);
        }
        else
        {
            lcd_lib_draw_string_center_atP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT+29, LCD_LINE_HEIGHT+LCD_LINE_HEIGHT/2+2, PSTR("CHANGE"));
            lcd_lib_draw_string_center_atP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT+29, 2*LCD_LINE_HEIGHT+LCD_LINE_HEIGHT/2+2, PSTR("MATERIAL"));
            lcd_lib_draw_gfx(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT+26, 3*LCD_LINE_HEIGHT+LCD_LINE_HEIGHT/2+2, filamentGfx);
        }
    }
    else if (nr == index++)
    {
        LCDMenu::drawMenuBox(LCD_CHAR_MARGIN_LEFT*2
                           , BOTTOM_MENU_YPOS
                           , 48
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_stringP(LCD_CHAR_MARGIN_LEFT + 3*LCD_CHAR_SPACING+1, BOTTOM_MENU_YPOS, PSTR("TUNE"));
            lcd_lib_clear_gfx(2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, menuGfx);
        }
        else
        {
            lcd_lib_draw_stringP(LCD_CHAR_MARGIN_LEFT + 3*LCD_CHAR_SPACING+1, BOTTOM_MENU_YPOS, PSTR("TUNE"));
            lcd_lib_draw_gfx(2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, menuGfx);
        }
    }
    else if (IS_SD_PRINTING && (nr == index++))
    {
        LCDMenu::drawMenuBox(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT*2
                                , BOTTOM_MENU_YPOS
                                , 48
                                , LCD_CHAR_HEIGHT
                                , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_stringP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 3*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, PSTR("ABORT"));
            lcd_lib_clear_gfx(LCD_GFX_WIDTH/2 + 10, BOTTOM_MENU_YPOS, standbyGfx);
        }
        else
        {
            lcd_lib_draw_stringP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 3*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, PSTR("ABORT"));
            lcd_lib_draw_gfx(LCD_GFX_WIDTH/2 + 10, BOTTOM_MENU_YPOS, standbyGfx);
        }
    }
}

void lcd_menu_print_resume()
{
    lcd_lib_clear();
    lcd_lib_draw_vline(64, 5, 46);
    lcd_lib_draw_hline(3, 124, 50);

    uint8_t len = IS_SD_PRINTING ? 4 : 2;

    menu.process_submenu(get_resume_menuoption, len);

    for (uint8_t index=0; index<len; ++index)
    {
        menu.drawSubMenu(drawResumeSubmenu, index);
    }
    lcd_lib_update_screen();
}

#endif//ENABLE_ULTILCD2
