#ifndef _SWITCH_H_
#define _SWITCH_H_


// ---------------------------------------------------------------------------
//
// Switch
//
// Virtual Interface for switch classes
//
// ---------------------------------------------------------------------------
class Switch {

public: 

    virtual unsigned int set(unsigned int) = 0;
    virtual unsigned int increment() = 0;
    virtual unsigned int get() = 0;

};


#endif // _SWITCH_H_
