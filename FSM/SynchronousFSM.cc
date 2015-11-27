// Copyright (c) 2015 Delta Prime, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// A very simple sample function and appropriate classes to show how a
// synchronous finite state machine (FSM) may be implemented.

#include <iostream>

#include "FiniteStateMachine.hh"


// The namespace where the FiniteStateMachine lives.
//
using namespace Core;


// A very basic audio player model which can be stopped or playing a song.

namespace AudioPlayer
{
   /**
    * The subject matter information as the name implies holds the domain
    * specific information that the state machine is operating on. This is
    * not strictly necessary, but it can be helpful.
    */
   struct SubjectMatterInformation
   {
      std::string song;    /// The song to play.
   };


   /**
    * The state machine of the AudioPlayer object. This is the class in which
    * activity actually occurs.
    */
   class Machine
      :  public FiniteStateMachine<>
   {
   public :

      Machine();
      virtual ~Machine();

      // The calls that the FSM receives from the outside world. They must be
      // converted to appropriate events and delivered to the current state.

      /**
       * Called by the external system when it needs to activate the player.
       */
      void PushPlay();

      /**
       * Called by the external system when it needs to deactivate the player.
       */
      void PushStop();


   private :

      // The state machine's event set.
      //
      enum
      {
         START_PLAYING_EVENT,
         STOP_PLAYING_EVENT
      };


   private :
      // The domain specific information block.
      //
      SubjectMatterInformation info;

      // These store the state numbers associated with the state objects that
      // are to be installed.
      //
      StateID STOPPED_STATE;
      StateID PLAYING_STATE;

      // The states supported by this machine.
      //
      State stopped;
      State playing;


   private :
      // These methods set the entry/exit function objects and install the
      // event handlers and transitions.
      //
      void ConfigureStoppedState();
      void ConfigurePlayingState();

      // The work is being done in the FSM not in the individual states.
      // These methods implement algorithms that the states' event handlers can
      // call on event arrival.
      //
      void PerformPlay();
      void PerformStop();
   };
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
AudioPlayer::Machine::Machine()
{
   // The single song this player plays.
   //
   info.song = "It's a Wonderful World";

   // The supported states must be registered.
   //
   STOPPED_STATE = RegisterState( & stopped );
   PLAYING_STATE = RegisterState( & playing );

   // Establish the starting state, this is important.
   //
   SetInitialState( STOPPED_STATE );

   // Configure the states' event handlers and transitions.
   //
   ConfigureStoppedState();
   ConfigurePlayingState();
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
AudioPlayer::Machine::~Machine()
{
}


//------------------------------------------------------------------------------
// This method is always called by a foreign object, so it is necessary to
// call the DeliverNextEvent() method as the last step. This is what drives
// the state machine and this is what makes the state machine 'synchronous'.
//------------------------------------------------------------------------------
void
AudioPlayer::Machine::PushPlay()
{
   // The event to post that will be delivered momentarily.
   //
   PostEvent( START_PLAYING_EVENT );

   // Event deliveries occur here until all (internal) events have been
   // processed. The only reason this method can be called here is because the
   // internals of this machine never call this method.
   //
   while ( DeliverNextEvent() );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioPlayer::Machine::PushStop()
{
   PostEvent( STOP_PLAYING_EVENT );
   while ( DeliverNextEvent() );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioPlayer::Machine::ConfigureStoppedState()
{
   stopped.SetEntryFunction(
      [this]( const Event & )
         {
            PerformStop();
         }
      );

   stopped.SetExitFunction(
      [this]( const Event & )
         {
            std::cout << "=== Special exit code executed for state: 'STOPPED' ===" << std::endl;
         }
      );

   stopped
      .SetEventHandler(
         STOP_PLAYING_EVENT,
         [this]( const Event & )
            {
               std::cout << "Already stopped, nothing to do." << std::endl;
            }
         )
      .SetTransition( START_PLAYING_EVENT, PLAYING_STATE );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioPlayer::Machine::ConfigurePlayingState()
{
   playing.SetEntryFunction(
      [this]( const Event & )
         {
            PerformPlay();
         }
      );

   playing.SetExitFunction(
      [this]( const Event & )
         {
            std::cout << "=== Special exit code executed for state: 'PLAYING' ===" << std::endl;
         }
      );

   playing
      .SetEventHandler(
         START_PLAYING_EVENT,
         [this]( const Event & )
            {
               std::cout << "Already playing, nothing to do." << std::endl;
            }
         )
      .SetTransition( STOP_PLAYING_EVENT, STOPPED_STATE );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioPlayer::Machine::PerformPlay()
{
   std::cout << "Playing '" << info.song << "'." << std::endl;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioPlayer::Machine::PerformStop()
{
   std::cout << "Stopped playing." << std::endl;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
RunSynchronousFSM()
{
   std::cout << "<<< Synchronous FSM Sample Started >>>" << std::endl << std::endl;

   AudioPlayer::Machine audio_player;

   audio_player.PushStop();
   audio_player.PushPlay();
   audio_player.PushPlay();
   audio_player.PushStop();

   std::cout << std::endl << "<<< Synchronous FSM Sample Finished >>>" << std::endl << std::endl;
}

