#include "commandbuffer.h"
#include "cardreader.h"
#include "ConfigurationDual.h"
#include "planner.h"
#include "stepper.h"
#include "machinesettings.h"
#include "UltiLCD2_low_lib.h"
#include "UltiLCD2_menu_print.h" // use lcd_cache as char buffer


CommandBuffer cmdBuffer;

#if (EXTRUDERS > 1)

#define TOOLCHANGE_DISTANCEX 8.0f
#define TOOLCHANGE_DISTANCEY 16.0f
//#define TOOLCHANGE_STARTY DUAL_Y_MIN_POS
#define WIPE_STARTX 45.0f
#define WIPE_DISTANCEX 33.0f
#define WIPE_DISTANCEY 4.0f

#if defined(TCSDSCRIPT)
#define CONFIG_DIR  "config"
#define FILENAME_T0 "T0"
#define FILENAME_T1 "T1"
#define FILENAME_WIPE "wipe"

CommandBuffer::~CommandBuffer()
{
    deleteScript(t0);
    deleteScript(t1);
    deleteScript(wipe);
}

void CommandBuffer::initScripts()
{
    // clear all
    deleteScript(t0);
    deleteScript(t1);
    deleteScript(wipe);
    t0=0;
    t1=0;
    wipe=0;

    if(!card.isOk())
    {
        card.initsd();
        if (!card.isOk())
        {
            return;
        }
    }

    card.setroot();
    {
        // change to config dir
        char filename[16];
        strcpy_P(filename, PSTR(CONFIG_DIR));
        card.chdir(filename);
        // read scripts from sd card
        strcpy_P(filename, PSTR(FILENAME_T0));
        if ((t0 = readScript(filename)));
        strcpy_P(filename, PSTR(FILENAME_T1));
        if ((t1 = readScript(filename)));
        strcpy_P(filename, PSTR(FILENAME_WIPE));
        if ((wipe = readScript(filename)));
    }
    card.setroot();
}

struct CommandBuffer::t_cmdline* CommandBuffer::readScript(const char *filename)
{
    struct t_cmdline* script(0);

    card.openFile(filename, true);
    if (card.isFileOpen())
    {
        struct t_cmdline* cmd = script = createScript();
        char buffer[MAX_CMD_SIZE] = {0};

        while( !card.eof() )
        {
            // read next line from file
            int16_t len = card.fgets(buffer, sizeof(buffer)-1);
            if (len <= 0) break;
            SERIAL_ECHO_START;
            SERIAL_ECHOLN(buffer);
            // remove trailing spaces
            while (len > 0 && buffer[len-1] <= ' ') buffer[--len] = '\0';
            if (len > 0)
            {
                if (cmd->str)
                {
                    // append line
                    cmd->next = createScript();
                    cmd = cmd->next;
                }
                cmd->str = new char[len+1];
                strncpy(cmd->str, buffer, len);
                cmd->str[len] = '\0';
            }
        }
        card.closefile();
    }
    if (script && !script->str && !script->next)
    {
        // no need to buffer empty files
        delete script;
        script = 0;
    }
    return script;
}

struct CommandBuffer::t_cmdline* CommandBuffer::createScript()
{
    struct t_cmdline* script = new t_cmdline;
    script->str = 0;
    script->next = 0;
    return script;
}

void CommandBuffer::deleteScript(struct t_cmdline *script)
{
    struct t_cmdline *cmd(script);
    while (cmd)
    {
        script = cmd->next;
        delete cmd->str;
        delete cmd;
        cmd = script;
    }
}

uint8_t CommandBuffer::processScript(struct t_cmdline *script)
{
    uint8_t cmdCount(0);
    while (script)
    {
        process_command(script->str, false);
        script = script->next;
        ++cmdCount;
        // update loop
        idle();
        checkHitEndstops();
    }
    return cmdCount;
}
#endif //TCSDSCRIPT

FORCE_INLINE void relative_e_move(const float eDiff, const float feedrate, uint8_t e)
{
    current_position[E_AXIS] += eDiff;
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], feedrate, e);
}

static void toolchange_retract(float x, float y, int feedrate, uint8_t e)
{
    if (!TOOLCHANGE_RETRACTED(e))
    {
        toolchange_recover_length[e] = toolchange_retractlen[e] / volume_to_filament_length[e];
#ifdef FWRETRACT
        if (EXTRUDER_RETRACTED(e))
        {
            toolchange_recover_length[e] = max(0.0, toolchange_recover_length[e]-retract_recover_length[e]);
        }
        SET_TOOLCHANGE_RETRACT(e);
#endif // FWRETRACT
        current_position[E_AXIS] -= toolchange_recover_length[e];
    }
    current_position[X_AXIS] = x;
    current_position[Y_AXIS] = y;
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], feedrate, e);
}

void CommandBuffer::processT0(bool bRetract, bool bWipe)
{
#if defined(SDSUPPORT) && defined(TCSDSCRIPT)
    if (t0)
    {
        processScript(t0);
        if (bRetract)
        {9
            toolchange_retract(current_position[X_AXIS], current_position[Y_AXIS], toolchange_retractfeedrate[1]/60, 1);
        }
    }
    else
#endif // SDSUPPORT
    {
        //float ypos = min(current_position[Y_AXIS], dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY);
        float ypos = dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY;
        /*if ((IS_WIPE_ENABLED && current_position[X_AXIS] < wipe_position[X_AXIS]))
        {
            ypos = dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY - roundOffset(Y_AXIS, extruder_offset[Y_AXIS][active_extruder]);
        }*/

        if (bRetract)
        {
            toolchange_retract(dock_position[X_AXIS] - TOOLCHANGE_DISTANCEX, ypos, 200, 1);
        }
        else
        {
            CommandBuffer::moveHead(dock_position[X_AXIS] - TOOLCHANGE_DISTANCEX, ypos, 200);
        }
        CommandBuffer::moveHead(current_position[X_AXIS], dock_position[Y_AXIS], 150);
        CommandBuffer::moveHead(dock_position[X_AXIS], dock_position[Y_AXIS], 100);
        CommandBuffer::moveHead(current_position[X_AXIS], dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY, 50);
        CommandBuffer::moveHead(dock_position[X_AXIS] - TOOLCHANGE_DISTANCEX, current_position[Y_AXIS], 100);
        if (!bWipe && !bRetract)
        {
            CommandBuffer::moveHead(dock_position[X_AXIS] - TOOLCHANGE_DISTANCEX, current_position[Y_AXIS], 200);
        }
  }
}

void CommandBuffer::processT1(bool bRetract, bool bWipe)
{
#if defined(SDSUPPORT) && defined(TCSDSCRIPT)
    if (t1)
    {
        processScript(t1);
        if (bRetract)
        {
            toolchange_retract(current_position[X_AXIS], current_position[Y_AXIS], toolchange_retractfeedrate[0]/60, 0);
        }
    }
    else
#endif // SDSUPPORT
    {
        CommandBuffer::move2dock(bRetract);
        CommandBuffer::moveHead(dock_position[X_AXIS] - TOOLCHANGE_DISTANCEX, dock_position[Y_AXIS], 100);

        if (!bWipe && !bRetract)
        {
            CommandBuffer::moveHead(dock_position[X_AXIS] - TOOLCHANGE_DISTANCEX, dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY, 200);
        }
  }
}

float CommandBuffer::preparePriming(uint8_t e)
{
#ifdef FWRETRACT
    float length = TOOLCHANGE_RETRACTED(e) ? toolchange_recover_length[e] : toolchange_retractlen[e]/volume_to_filament_length[e];
    CLEAR_TOOLCHANGE_RETRACT(e);
    toolchange_recover_length[e] = 0.0f;
    if (EXTRUDER_RETRACTED(e))
    {
        length += retract_recover_length[e];
    }
#else
    float length = toolchange_retractlen[e]/volume_to_filament_length[e];
#endif // FWRETRACT

    // undo the toolchange retraction
    relative_e_move(length*0.8, toolchange_retractfeedrate[e]/60, e);
    return (length*0.2);
}

void CommandBuffer::processWipe(const uint8_t printState)
{
    float length = CommandBuffer::preparePriming(active_extruder);

    // prime nozzle
    relative_e_move(length+toolchange_prime[active_extruder]/volume_to_filament_length[active_extruder], 0.5f, active_extruder);
    primed |= (EXTRUDER_PRIMED << active_extruder);
    primed |= ENDOFPRINT_RETRACT;
    // retract before wipe
    if (!EXTRUDER_RETRACTED(active_extruder))
    {
        retract_recover_length[active_extruder] = retract_length/volume_to_filament_length[active_extruder];
        SET_EXTRUDER_RETRACT(active_extruder);
    }
    relative_e_move(-retract_recover_length[active_extruder], retract_feedrate/60, active_extruder);

    // wait a short moment
    st_synchronize();
    dwell(250);

#if defined(SDSUPPORT) && defined(TCSDSCRIPT)
    if (wipe)
    {
        processScript(wipe);
    if (printing_state < PRINT_STATE_ABORT)
    {
      // switch fan speed back to normal
      printing_state = printState;
      check_axes_activity();
    }
    }
    else
#endif // SDSUPPORT
    {
        // wipe start position
        CommandBuffer::moveHead(current_position[X_AXIS]-WIPE_STARTX, current_position[Y_AXIS], 200);

        // slow wipe move
        CommandBuffer::moveHead(current_position[X_AXIS]-WIPE_DISTANCEX, current_position[Y_AXIS], 40);

    if (printing_state < PRINT_STATE_ABORT)
    {
      // switch fan speed back to normal
      printing_state = printState;
      check_axes_activity();
    }

        // snip move
        CommandBuffer::moveHead(current_position[X_AXIS], current_position[Y_AXIS]+WIPE_DISTANCEY, 125);
        // diagonal move
        CommandBuffer::moveHead(current_position[X_AXIS]+WIPE_DISTANCEY, current_position[Y_AXIS]+WIPE_DISTANCEY, 125);
        // back to start position
        CommandBuffer::moveHead(wipe_position[X_AXIS], dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY, 200);
  }
}
#endif // EXTRUDERS

void CommandBuffer::moveHead(float x, float y, int feedrate)
{
    current_position[X_AXIS] = x;
    current_position[Y_AXIS] = y;
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], feedrate, active_extruder);
}

#if EXTRUDERS > 1
void CommandBuffer::move2dock(bool bRetract)
{
    /*if (current_position[Y_AXIS] < dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY)
    {
        CommandBuffer::moveHead(current_position[X_AXIS], dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY, 200);
    }*/
    if (bRetract)
    {
        toolchange_retract(dock_position[X_AXIS] - TOOLCHANGE_DISTANCEX, dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY, 200, 0);
        CommandBuffer::moveHead(dock_position[X_AXIS], dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY, 1000);
    }
    else
    {
        CommandBuffer::moveHead(dock_position[X_AXIS] - TOOLCHANGE_DISTANCEX, dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY, 200);
        CommandBuffer::moveHead(dock_position[X_AXIS], dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY, 200);
    }
    CommandBuffer::moveHead(dock_position[X_AXIS], dock_position[Y_AXIS], 1000);
}
#endif // EXTRUDERS

void CommandBuffer::move2heatup()
{
    float x, y;
#if (EXTRUDERS > 1)
    if IS_DUAL_ENABLED
    {
        /*x = wipe_position[X_AXIS]+roundOffset(X_AXIS, extruder_offset[X_AXIS][active_extruder]);
        if (current_position[Y_AXIS] >= dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY)
        {
        // y = 65.0f;
            CommandBuffer::moveHead(x, dock_position[Y_AXIS] + TOOLCHANGE_DISTANCEY, 200);
        }*/
        x = max_pos[X_AXIS] - 10.0;
        y = min_pos[Y_AXIS] + 10.0;
    }
    else
    {
        x = max(5.0f, min_pos[X_AXIS] + roundOffset(X_AXIS, extruder_offset[X_AXIS][active_extruder]) + 5);
        y = min_pos[Y_AXIS] + 10.0;
    }
#else
    x = max(5.0f, min_pos[X_AXIS] + 5);
    y = min_pos[Y_AXIS] + 10.0;
#endif
    CommandBuffer::moveHead(x, y, 200);
}

void CommandBuffer::move2front()
{
#if (EXTRUDERS > 1)
    float x = IS_DUAL_ENABLED ? wipe_position[X_AXIS]+roundOffset(X_AXIS, extruder_offset[X_AXIS][active_extruder]) : AXIS_CENTER_POS(X_AXIS);
    float y = IS_DUAL_ENABLED ? int(min_pos[Y_AXIS])+10 : int(min_pos[Y_AXIS])+10;
#else
    float x = AXIS_CENTER_POS(X_AXIS);
    float y = int(min_pos[Y_AXIS])+10;
#endif
    CommandBuffer::moveHead(x, y, 200);
}

#if (EXTRUDERS > 1)
// move to a safe y position in dual mode
/*void CommandBuffer::move2SafeYPos()
{
    if (IS_DUAL_ENABLED && (position_state & KNOWNPOS_Y) && current_position[Y_AXIS] < DUAL_Y_MIN_POS)
    {
        moveHead(current_position[X_AXIS], DUAL_Y_MIN_POS, 120);
    }
}*/
#endif

void CommandBuffer::homeHead()
{
    enquecommand_P(PSTR("G28 X0 Y0"));
}

void CommandBuffer::homeBed()
{
    enquecommand_P(PSTR("G28 Z0"));
}

void CommandBuffer::homeAll()
{
    enquecommand_P(PSTR("G28"));
}

void CommandBuffer::dwell(const unsigned long m)
{
    unsigned long target_millis = millis() + m;
    while(millis() < target_millis )
    {
        idle();
    }
}
