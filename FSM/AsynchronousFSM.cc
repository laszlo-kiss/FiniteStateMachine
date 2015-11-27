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

// A very simple sample function and appropriate classes to show how an
// asynchronous finite state machine (FSM) may be implemented.

#include <iostream>
#include <thread>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "FiniteStateMachine.hh"


// The namespace where the FiniteStateMachine lives.
//
using namespace Core;

// A very basic audio recorder model which can be stopped, playing or recording.
// It can also skip to the next recording.

namespace AudioRecorder
{
   /**
    * The subject matter information as the name implies holds the domain
    * specific information that the state machine is operating on. This is
    * not strictly necessary, but it can be helpful.
    */
   struct SubjectMatterInformation
   {
      using SongName = std::string;
      using SongDuration = int;

      std::deque< std::tuple< SongName, SongDuration > > songs;

      int current_song;       // the one that is about to play (or is playing)
      int next_song_number;   // for numbering recordings

      SubjectMatterInformation() : current_song( 0 ), next_song_number( 1 ) { }
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

      /**
       * Starts the thread on which this machine is executing.
       */
      void StartThread();

      /**
       * Stops the thread.
       */
      void StopThread();


      /**
       * Override to allow this machine to post the event to the thread in a
       * safe manner. This is what makes the machine async capable.
       */
      void EventDeliveryMethod( const Event & event ) override;


      // The calls that the FSM receives from the outside world. They must be
      // converted to appropriate events and delivered to the current state.

      /**
       * Called by the external system when it needs to activate the player.
       */
      void PushPlay() { PostEvent( START_PLAYING_EVENT ); }


      /**
       * Called by the external system when it needs to deactivate the player.
       */
      void PushStop() { PostEvent( STOP_PLAYING_EVENT ); }


      /**
       * Called by the external system to start recording a song.
       */
      void PushRecord() { PostEvent( START_RECORDING_EVENT ); }


      /**
       * Called by the external system to skip to the next song.
       */
      void PushSkipForward() { PostEvent( ADVANCE_TO_NEXT_EVENT ); }


   private :

      // The state machine's event set.
      //
      enum
      {
         START_PLAYING_EVENT,
         STOP_PLAYING_EVENT,
         START_RECORDING_EVENT,
         ADVANCE_TO_NEXT_EVENT,
         TIMEOUT_EVENT
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
      StateID RECORDING_STATE;

      // The states supported by this machine.
      //
      State stopped;
      State playing;
      State recording;

      // These two form the basis of the Boost asynch I/O framework, which is
      // good for event driven work.
      //
      boost::asio::io_service       io;
      boost::asio::io_service::work work;

      // The thread in which the recorder is operating.
      //
      std::thread thread;

      // Use a timer to pretend that songs are playing for some variable amount
      // of time.
      //
      boost::asio::deadline_timer song_timer;


   private :

      // These methods set the entry/exit function objects and install the
      // event handlers and transitions.
      //
      void ConfigureStoppedState();
      void ConfigurePlayingState();
      void ConfigureRecordingState();

      // The methods that actually perform the functions requested by the
      // external system, but in the context of the thread.
      //
      // The work is being done in the FSM not in the individual states.
      // These methods implement algorithms that the states' event handlers can
      // call on event arrival.
      //
      void PerformPlay();
      void PerformStop();
      void PerformRecord();
      void PerformSkip();

      // A utility method to go to the next song.
      //
      void AdvanceToNextSong();

      // Accomodate the operating convention of the boost deadline timer.
      //
      void PerformTimeout( const boost::system::error_code & error )
      {
         if ( error != boost::asio::error::operation_aborted ) PostEvent( TIMEOUT_EVENT );
      }
   };
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
AudioRecorder::Machine::Machine()
   :  io()
   ,  work( io )
   ,  thread()
   ,  song_timer( io )
{
   // The songs to play.
   //
   info.songs.push_back( { "Heartbeat Song", 2 } );
   info.songs.push_back( { "One Hot Mess", 3 } );
   info.songs.push_back( { "Cool", 2 } );

   // The supported states must be registered.
   //
   STOPPED_STATE = RegisterState( & stopped );
   PLAYING_STATE = RegisterState( & playing );
   RECORDING_STATE = RegisterState( & recording );

   // Establish the starting state, this is important.
   //
   SetInitialState( STOPPED_STATE );

   // Configure the states' event handlers and transitions.
   //
   ConfigureStoppedState();
   ConfigurePlayingState();
   ConfigureRecordingState();
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
AudioRecorder::Machine::~Machine()
{
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioRecorder::Machine::StartThread()
{
   try
   {
      this->thread = std::thread( [this](){ this->io.run(); } );
   }
   catch ( const std::system_error & e )
   {
      std::cout << "ERROR: Failed to spawn thread. [" << e.code() << "] " << e.what();
      throw;
   }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioRecorder::Machine::StopThread()
{
   io.stop();
   if ( thread.joinable() )
   {
      thread.join();
   }
}


//------------------------------------------------------------------------------
// The event here is posted to the thread for execution.
//------------------------------------------------------------------------------
void
AudioRecorder::Machine::EventDeliveryMethod( const Event & event )
{
   io.post(
      [this, event]()
         {
            FiniteStateMachine<>::EventDeliveryMethod( event );
         }
      );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioRecorder::Machine::ConfigureStoppedState()
{
   stopped.SetEntryFunction(
      [this]( const Event & )
         {
            std::cout << "=== Special entry code executed for state: 'STOPPED' ===" << std::endl;
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
      .SetEventHandler(
         ADVANCE_TO_NEXT_EVENT,
         [this]( const Event & )
            {
               std::cout << "Stopped, can't skip." << std::endl;
            }
         )
      .SetTransition( START_PLAYING_EVENT, PLAYING_STATE )
      .SetTransition( START_RECORDING_EVENT, RECORDING_STATE );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioRecorder::Machine::ConfigurePlayingState()
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
            song_timer.cancel();
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
      .SetEventHandler(
         START_RECORDING_EVENT,
         [this]( const Event & )
            {
               std::cout << "Playing, can't record." << std::endl;
            }
         )
      .SetEventHandler(
         ADVANCE_TO_NEXT_EVENT,
         [this]( const Event & )
            {
               std::cout << "Skipping..." << std::endl;
               PerformSkip();
               PerformPlay();
            }
         )
      .SetEventHandler(
         TIMEOUT_EVENT,
         [this]( const Event & )
            {
               AdvanceToNextSong();
               PerformPlay();
            }
         )
      .SetTransition( STOP_PLAYING_EVENT, STOPPED_STATE );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioRecorder::Machine::ConfigureRecordingState()
{
   recording.SetEntryFunction(
      [this]( const Event & )
         {
            PerformRecord();
         }
      );

   recording.SetExitFunction(
      [this]( const Event & )
         {
            song_timer.cancel();
         }
      );

   recording
      .SetEventHandler(
            START_PLAYING_EVENT,
            [this]( const Event & event )
               {
                  std::cout << "Recording, can't play." << std::endl;
               }
            )
      .SetEventHandler(
         START_RECORDING_EVENT,
         [this]( const Event & event )
            {
               std::cout << "Already recording, nothing to do." << std::endl;
            }
         )
      .SetEventHandler(
         ADVANCE_TO_NEXT_EVENT,
         [this]( const Event & )
            {
               std::cout << "Recording, can't skip." << std::endl;
            }
         )
      .SetEventHandler(
         TIMEOUT_EVENT,
         [this]( const Event & event )
            {
               PerformRecord();
            }
         )
      .SetTransition( STOP_PLAYING_EVENT, STOPPED_STATE );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioRecorder::Machine::PerformPlay()
{
   std::cout << "Playing '" << std::get<0>(info.songs[info.current_song]) << "'." << std::endl;

   const int duration = std::get<1>(info.songs[info.current_song]);

   song_timer.expires_from_now( boost::posix_time::seconds(duration) );
   song_timer.async_wait(
      boost::bind(
         & AudioRecorder::Machine::PerformTimeout,
         this,
         boost::asio::placeholders::error
         )
      );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioRecorder::Machine::PerformStop()
{
   std::cout << "Stopped playing." << std::endl;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioRecorder::Machine::PerformRecord()
{
   std::ostringstream ostr;
   ostr << "Recorded Song " << info.next_song_number++;
   const int duration = 2 + (info.next_song_number % 2);
   info.songs.push_back( { ostr.str(), duration } );

   std::cout << "Recording '" << ostr.str() << "'." << std::endl;

   song_timer.expires_from_now( boost::posix_time::seconds(duration) );
   song_timer.async_wait(
      boost::bind(
         & AudioRecorder::Machine::PerformTimeout,
         this,
         boost::asio::placeholders::error
         )
      );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioRecorder::Machine::PerformSkip()
{
   song_timer.cancel();
   AdvanceToNextSong();
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
AudioRecorder::Machine::AdvanceToNextSong()
{
   info.current_song++;
   if ( info.current_song > (info.songs.size() - 1) )
   {
      info.current_song = 0;
   }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
RunAsynchronousFSM()
{
   std::cout << "<<< Asynchronous FSM Sample Started >>>" << std::endl << std::endl;

   AudioRecorder::Machine audio_recorder;

   audio_recorder.StartThread();

   sleep( 2 );
   audio_recorder.PushStop();
   sleep( 2 );
   audio_recorder.PushPlay();
   sleep( 10 );
   audio_recorder.PushSkipForward();
   sleep( 5 );
   audio_recorder.PushRecord();
   sleep( 1 );
   audio_recorder.PushStop();
   audio_recorder.PushRecord();
   sleep( 5 );
   audio_recorder.PushPlay();
   sleep( 2 );
   audio_recorder.PushStop();
   sleep( 2 );

   audio_recorder.PushPlay();
   sleep( 15 );
   audio_recorder.PushStop();
   sleep( 1 );

   audio_recorder.StopThread();

   std::cout << std::endl << "<<< Asynchronous FSM Sample Finished >>>" << std::endl << std::endl;
}

