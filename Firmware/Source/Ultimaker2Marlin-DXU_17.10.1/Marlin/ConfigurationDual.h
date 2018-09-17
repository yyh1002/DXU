#ifndef UM2_DUAL_H
#define UM2_DUAL_H

#include "Configuration.h"

#if (EXTRUDERS > 1)

#define EEPROM_ADDHOMEING_Z2     0x606  //  4 Byte
#define EEPROM_EXTRUDER_OFFSET   0x60A  // 16 Byte
#define EEPROM_DOCK_POSITION     0x61A  //  8 Byte
#define EEPROM_WIPE_POSITION     0x622  //  8 Byte
#define EEPROM_DUAL_STATE        0x62A  //  1 Byte
#define EEPROM_DUAL_RETRACTLEN   0x62B  //  8 Byte toolchange_retract_length
#define EEPROM_DUAL_RETRACTSPEED 0x633  //  8 Byte toolchange_retract_feedrate
#define EEPROM_DUAL_EXTRAPRIME   0x63B  //  8 Byte toolchange_prime
#define EEPROM_DUAL_RESERVED     0x643  // next address

// dual state flags
#define DUAL_ENABLED         1
#define DUAL_WIPE            2
#define DUAL_TOOLCHANGE      4

extern uint8_t dual_state;
extern float add_homeing_z2;
extern float dock_position[2];
extern float wipe_position[2];
extern float toolchange_retractlen[EXTRUDERS];
extern float toolchange_retractfeedrate[EXTRUDERS];
extern float toolchange_prime[EXTRUDERS];
extern float toolchange_recover_length[EXTRUDERS];

#ifdef EEPROM_CHITCHAT
void Dual_PrintSettings();
#else
FORCE_INLINE void Dual_PrintSettings() {}
#endif

#ifdef EEPROM_SETTINGS
void Dual_RetrieveSettings();
void Dual_ClearStorage();
FORCE_INLINE void Dual_StoreState() { eeprom_write_byte((uint8_t*)EEPROM_DUAL_STATE, dual_state); }
FORCE_INLINE void Dual_StoreExtruderOffset() { eeprom_write_block(extruder_offset, (uint8_t*)EEPROM_EXTRUDER_OFFSET, sizeof(extruder_offset)); }
FORCE_INLINE void Dual_StoreDockPosition() { eeprom_write_block(dock_position, (uint8_t*)EEPROM_DOCK_POSITION, sizeof(dock_position)); }
FORCE_INLINE void Dual_StoreWipePosition() { eeprom_write_block(wipe_position, (uint8_t*)EEPROM_WIPE_POSITION, sizeof(wipe_position)); }
FORCE_INLINE void Dual_StoreAddHomeingZ2() { eeprom_write_float((float*)EEPROM_ADDHOMEING_Z2, add_homeing_z2); }
FORCE_INLINE void Dual_StoreRetract()
{
    eeprom_write_block(toolchange_retractlen, (uint8_t*)EEPROM_DUAL_RETRACTLEN, sizeof(toolchange_retractlen));
    eeprom_write_block(toolchange_retractfeedrate, (uint8_t*)EEPROM_DUAL_RETRACTSPEED, sizeof(toolchange_retractfeedrate));
    eeprom_write_block(toolchange_prime, (uint8_t*)EEPROM_DUAL_EXTRAPRIME, sizeof(toolchange_prime));
}
#else
FORCE_INLINE void Dual_RetrieveSettings() {}
FORCE_INLINE void Dual_ClearStorage() {}
FORCE_INLINE void Dual_StoreExtruderOffset() {}
FORCE_INLINE void Dual_StoreDockPosition() {}
FORCE_INLINE void Dual_StoreWipePosition() {}
FORCE_INLINE void Dual_StoreAddHomeingZ2() {}
FORCE_INLINE void Dual_StoreRetract() {}
#endif

#define IS_DUAL_ENABLED (dual_state & DUAL_ENABLED)
#define IS_TOOLCHANGE_ENABLED ((dual_state & DUAL_ENABLED) && (dual_state & DUAL_TOOLCHANGE))
#define IS_WIPE_ENABLED ((dual_state & DUAL_ENABLED) && (dual_state & DUAL_WIPE))

#endif //EXTRUDERS

#endif //UM2_DUAL_H
