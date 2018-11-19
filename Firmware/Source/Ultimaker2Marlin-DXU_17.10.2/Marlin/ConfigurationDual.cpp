#include "Marlin.h"
#include "ConfigurationDual.h"

#if (EXTRUDERS > 1)
#include "UltiLCD2_hi_lib.h"

//Random number to verify that the dual settings are already written to the EEPROM
#define EEPROM_DUAL_MAGIC 0x218DE93C

#define EEPROM_DUAL_START        0x600  //  4 Byte Magic number
#define EEPROM_DUAL_VERSION      0x604  //  2 Byte

// IMPORTANT:  Whenever there are changes made to the variables stored in EEPROM
// in the functions below, also increment the version number. This makes sure that
// the default values are used whenever there is a change to the data, to prevent
// wrong data being written to the variables.
#define STORE_DUAL_VERSION 3

#define DOCK_X_POSITION    218.5
#define DOCK_Y_POSITION     203.0

#define WIPE_X_POSITION    105
#define WIPE_Y_POSITION     27

#define TOOLCHANGE_RETRACT_LENGTH  16
#define TOOLCHANGE_RETRACT_SPEED   18*60

uint8_t dual_state = DUAL_ENABLED | DUAL_TOOLCHANGE | DUAL_WIPE;
float add_homeing_z2 = 0.0f;
float dock_position[2] = {0.0f, 0.0f};
float wipe_position[2] = {0.0f, 0.0f};
float toolchange_retractlen[EXTRUDERS] = ARRAY_BY_EXTRUDERS(0.0f, 0.0f, 0.0f);
float toolchange_retractfeedrate[EXTRUDERS] = ARRAY_BY_EXTRUDERS(0.0f, 0.0f, 0.0f);
float toolchange_prime[EXTRUDERS] = ARRAY_BY_EXTRUDERS(0.0f, 0.0f, 0.0f);
float toolchange_recover_length[EXTRUDERS] = ARRAY_BY_EXTRUDERS(0.0f, 0.0f, 0.0f);

#ifdef EEPROM_SETTINGS
static bool Dual_RetrieveVersion(uint16_t &version)
{
    uint32_t magic = eeprom_read_dword((uint32_t*)(EEPROM_DUAL_START));
    if (magic == EEPROM_DUAL_MAGIC)
    {
        version = eeprom_read_word((const uint16_t*)EEPROM_DUAL_VERSION);
        return true;
    }
    return false;
}

void Dual_ClearStorage()
{
    // invalidate data
    eeprom_write_dword((uint32_t*)(EEPROM_DUAL_START), 0);
}

void Dual_RetrieveSettings()
{
    uint16_t version;
    bool bValid = Dual_RetrieveVersion(version);
    if (!bValid)
    {
        add_homeing_z2 = add_homeing[Z_AXIS];
        Dual_StoreAddHomeingZ2();
        Dual_StoreExtruderOffset();
    }
    else
    {
        add_homeing_z2 = eeprom_read_float((const float*)EEPROM_ADDHOMEING_Z2);
        eeprom_read_block(extruder_offset, (uint8_t*)EEPROM_EXTRUDER_OFFSET, sizeof(extruder_offset));
    }
    if (!bValid || version < 1)
    {
        dock_position[X_AXIS] = DOCK_X_POSITION;
        dock_position[Y_AXIS] = DOCK_Y_POSITION;
        Dual_StoreDockPosition();
        wipe_position[X_AXIS] = WIPE_X_POSITION;
        wipe_position[Y_AXIS] = WIPE_Y_POSITION;
        Dual_StoreWipePosition();
    }
    else
    {
        eeprom_read_block(dock_position, (uint8_t*)EEPROM_DOCK_POSITION, sizeof(dock_position));
        eeprom_read_block(wipe_position, (uint8_t*)EEPROM_WIPE_POSITION, sizeof(wipe_position));

    }
    if (!bValid || version < 2)
    {
        dual_state = DUAL_ENABLED | DUAL_TOOLCHANGE;
        Dual_StoreState();

        for (uint8_t e=0; e<EXTRUDERS; ++e)
        {
            toolchange_retractlen[e]      = TOOLCHANGE_RETRACT_LENGTH;
            toolchange_retractfeedrate[e] = TOOLCHANGE_RETRACT_SPEED;
        }
    }
    else
    {
        dual_state = eeprom_read_byte((uint8_t*)EEPROM_DUAL_STATE);
        eeprom_read_block(toolchange_retractlen, (uint8_t*)EEPROM_DUAL_RETRACTLEN, sizeof(toolchange_retractlen));
        eeprom_read_block(toolchange_retractfeedrate, (uint8_t*)EEPROM_DUAL_RETRACTSPEED, sizeof(toolchange_retractfeedrate));
    }

    if (!bValid || version < 3)
    {
        for (uint8_t e=0; e<EXTRUDERS; ++e)
        {
            toolchange_prime[e] = toolchange_retractlen[e] * 0.0625;
        }
        Dual_StoreRetract();

        // write version
        uint16_t version = STORE_DUAL_VERSION;
        eeprom_write_word((uint16_t*)EEPROM_DUAL_VERSION, version);
        // validate data
        eeprom_write_dword((uint32_t*)(EEPROM_DUAL_START), EEPROM_DUAL_MAGIC);
    }
    else
    {
        eeprom_read_block(toolchange_prime, (uint8_t*)EEPROM_DUAL_EXTRAPRIME, sizeof(toolchange_prime));
    }
    Dual_PrintSettings();
}

#endif // EEPROM_SETTINGS


#ifdef EEPROM_CHITCHAT
void Dual_PrintSettings()
{  // Always have this function, even with EEPROM_SETTINGS disabled, the current values will be shown

    SERIAL_ECHO_START;
    SERIAL_ECHOPGM("Dual setup: ");
    if (dual_state & DUAL_ENABLED)
    {
        SERIAL_ECHOPGM("enabled");
    }
    else
    {
        SERIAL_ECHOPGM("disabled");
    }
    SERIAL_EOL;

    SERIAL_ECHO_START;
    SERIAL_ECHOPAIR("Home offset z2 (mm): ", add_homeing_z2);
    SERIAL_EOL;

    SERIAL_ECHO_START;
    SERIAL_ECHOPGM("Dock position:");
    SERIAL_ECHOPAIR(" X", dock_position[X_AXIS]);
    SERIAL_ECHOPAIR(" Y", dock_position[Y_AXIS]);
    SERIAL_EOL;

    SERIAL_ECHO_START;
    SERIAL_ECHOPGM("Firmware wipe: ");
    if (dual_state & DUAL_WIPE)
    {
        SERIAL_ECHOPGM("enabled");
    }
    else
    {
        SERIAL_ECHOPGM("disabled");
    }
    SERIAL_EOL;

    SERIAL_ECHO_START;
    SERIAL_ECHOPGM("Wipe position:");
    SERIAL_ECHOPAIR(" X", wipe_position[X_AXIS]);
    SERIAL_ECHOPAIR(" Y", wipe_position[Y_AXIS]);
    SERIAL_EOL;

}
#endif //EEPROM_CHITCHAT

#endif //EXTRUDERS
