#ifndef COMMANDBUFFER_H
#define COMMANDBUFFER_H

#include "Marlin.h"

class CommandBuffer
{
  public:

    static void homeHead();
    static void homeBed();
    static void homeAll();
    static void move2heatup();
    static void move2front();
    static void moveHead(float x, float y, int feedrate);
    static void dwell(const unsigned long m);

#if EXTRUDERS > 1
    //static void move2SafeYPos();
    static void move2dock(bool bRetract);
    static float preparePriming(uint8_t e);
    void processT0(bool bRetract, bool bWipe);
    void processT1(bool bRetract, bool bWipe);
    void processWipe(const uint8_t printState);
  #if defined(TCSDSCRIPT)
    // constructor
    CommandBuffer () : t0(0), t1(0), wipe(0)  {}
    // destructor
    ~CommandBuffer ();

    void initScripts();
  private:
    // the structure of a single node
    struct t_cmdline{
	  char *str;
	  struct t_cmdline *next;
	};

    // command scripts for extruder change
    struct t_cmdline *t0;
    struct t_cmdline *t1;
    struct t_cmdline *wipe;

  private:
    void deleteScript(struct t_cmdline *script);
    uint8_t processScript(struct t_cmdline *script);
    struct t_cmdline* createScript();
    struct t_cmdline* readScript(const char *filename);
  #else
    // constructor
    CommandBuffer() {}
    // destructor
    ~CommandBuffer() {}
  #endif //TCSDSCRIPT
#else
    // constructor
    CommandBuffer() {}
    // destructor
    ~CommandBuffer() {}
#endif // EXTRUDERS
};

extern CommandBuffer cmdBuffer;

#endif //COMMANDBUFFER_H
