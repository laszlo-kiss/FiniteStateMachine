#include <assert.h>
#include <iostream>

/**
 * A sample function that exercises a fictional FSM based on a rudimentary
 * audio player.
 */
extern void RunSynchronousFSM();

/**
 * A sample function that exercises a fictional asynchronous FSM based on a
 * an audio recorder.
 */
extern void RunAsynchronousFSM();


int main ( int argc, const char * argv[] )
{
   RunSynchronousFSM();
   RunAsynchronousFSM();
   return 0;
}

