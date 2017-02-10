#pragma once

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

/** 
 * The definition of a Finite State Machine (FSM) and corresponding state
 * object.
 *
 * These classes allow the user to create state machines fairly simply.
 *
 * The states are very simple, they include a dispatch table for the events
 * to which they respond and they can optionally have entry and exit
 * methods. The Finite State Machine proper is responsible for actually
 * driving the state machine. To keep things simple that class can also be the
 * one that implements domain specific methods which the state's event handlers
 * call on.
 *
 * There are two event queues, one external and one internal. The external
 * event queue is generally filled by events generted on behalf of the clients
 * of the FSM. The internal event queue allows the FSM to perform multiple
 * transitions in response to one external event and therefore has higher
 * priority.
 *
 *  @author: Laszlo Kiss
 *  @since: 5/1/2015
 */

#include <assert.h>
#include <stdexcept>
#include <functional>
#include <iterator>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <sstream>


// A namespace to partition the FSM class from the rest of the system.
//
namespace Core
{
   /**
    * The base class for any finite state machine implementation.
    * The Event template parameter is a type that may function as a key in
    * a std::map<> and which supports automatic type conversion to 'int'.
    * The HANDLER template parameter specifies the function object that is
    * to be used as the event handler.
    */
   template <
      typename EVENT = int,
      typename HANDLER = std::function< void ( const EVENT & ) >
      >
   class FiniteStateMachine
   {
   public :
      /**
       * A convenience typedef.
       */
      using Event = EVENT;

      /**
       * Events are queued to the state machine and stored in instances of this
       * type.
       */
      using Events = std::deque< Event >;

      /**
       * Collection of Events.
       */
      using EventSet = std::set< Event >;

      /**
       * An event handler is simply a function object that is called to handle
       * an event.
       */
      using EventHandler = HANDLER;


      /**
       * The EntryFunction is called when entering a new state.
       */
      using EntryFunction = std::function< void ( const Event & ) >;


      /**
       * The ExitFunction is called when leaving the current state.
       */
      using ExitFunction = std::function< void ( const Event & ) >;


      /**
       * An alternate mechanism to refer to a state object.
       */
      using StateID = int;


      /**
       * The exception thrown when the state id number is out of bounds.
       */
      class Exception
         :  public std::runtime_error
      {
      public :
         Exception( const std::string & why = "" )
            :  std::runtime_error( why )
         { }
      };


      /**
       * The base class of all State objects compatible with this FSM
       * implementation.
       */
      class State
      {
      public :
         /**
          * Default constructor sets all members to their default values.
          */
         State()
            :  entry_function()
            ,  exit_function()
            ,  dispatch_table()
            ,  single_dispatch()
            ,  default_handler()
            ,  transition_table()
            ,  forwarded_events()
            ,  event_store()
            ,  storeforward_table()
         { }


         virtual ~State() = default;


         /**
          * Sets the function the FSM calls when the state is entered.
          *
          * @param efun The entry function object to set.
          * @return The old entry function or nullptr if none.
          */
         virtual EntryFunction SetEntryFunction(
            const EntryFunction & efun
            )
         {
            EntryFunction old_function{};
            std::swap( old_function, entry_function );
            entry_function = efun;
            return std::move( old_function );
         }


         /**
          * Sets the function the FSM calls when the state is exited.
          *
          * @param efun The exit function object to set.
          * @return The old exit function or nullptr if none.
          */
         virtual ExitFunction SetExitFunction(
            const ExitFunction & efun
            )
         {
            ExitFunction old_function{};
            std::swap( old_function, exit_function );
            exit_function = efun;
            return std::move( old_function );
         }


         /**
          * Sets the state transition to take when a particular event arrives.
          *
          * @param event   The event that is to be handled.
          * @param state   The state to which to transition.
          * @return The reference to this State object.
          */
         virtual State & SetTransition(
            const Event & event,
            StateID state
            )
         {
            // Indicates that the event has already been configured in this
            // state. A single event can't have multiple functions.
            //
            assert( dispatch_table.find( event ) == dispatch_table.end() );

            if ( state < LowestValidStateID )
            {
               throw Exception("Out of bounds state id number.");
            }
            transition_table.insert( { event, state } );
            return *this;
         }


         /**
          * Replaces a an existing transition with another.
          * This is a 'fancy' capability that should be used with caution.
          *
          * @param event   The event that is to be handled.
          * @param handler The handler function object to replace the existing one.
          * @return the old state that was replaced or SentinelStateID indicating an error.
          */
         virtual StateID ReplaceTransition(
            const Event & event,
            StateID state
            )
         {
            StateID old_state{ SentinelStateID };
            auto dit = transition_table.find( event );
            if ( dit != transition_table.end() )
            {
               std::swap( dit->second, old_state );
               transition_table[event] = state;
            }
            return old_state;
         }


         /**
          * Removes an existing transition.
          * This is a 'fancy' capability that should be used with caution.
          *
          * @param event   The event that is to be removed from the transition table.
          * @return the old state to which the removed event was transitioning
          *    or SentinelStateID indicating that there was no such transition.
          */
         virtual StateID RemoveTransition(
            const Event & event
            )
         {
            StateID old_state{ SentinelStateID };
            auto dit = transition_table.find( event );
            if ( dit != transition_table.end() )
            {
               std::swap( dit->second, old_state );
               transition_table.erase( dit );
            }
            return old_state;
         }


         /**
          * Retrieves the target state if a transition was installed for the
          * provided event.
          *
          * @param event   The event for which the target state is needed.
          * @return the StateID of the target state or SentinelStateID if there is no such.
          */
         inline StateID TransitionForEvent(
            const Event & event
            ) const
         {
            StateID target{ SentinelStateID };
            auto dit = transition_table.find( event );
            if ( dit != transition_table.end() )
            {
               target = dit->second;
            }
            return target;
         }


         /**
          * Sets the event handler function the FSM calls when an event needs
          * to be handled.
          *
          * @param event   The event that is to be handled.
          * @param handler The handler function object to set.
          * @param single_dispatch_only If the handler can only be executed once.
          * @return The reference to this State object.
          */
         virtual State & SetEventHandler(
            const Event & event,
            const EventHandler & handler,
            bool single_dispatch_only = false
            )
         {
            // Indicates that the event has already been configured in this
            // state. A single event can't have multiple functions.
            //
            assert( transition_table.find( event ) == transition_table.end() );
            dispatch_table.insert( { event, handler } );
            if ( single_dispatch_only )
            {
               single_dispatch.insert( { event, nullptr } );
            }
            return *this;
         }


         /**
          * Sets the default event handler function of the state to use when an
          * event does not have a specific handler.
          *
          * @param handler The handler function object to set.
          */
         virtual State & SetDefaultEventHandler(
            const EventHandler & handler
            )
         {
            default_handler = handler;
            return *this;
         }


         /**
          * Replaces a potentially existing event handler function with another.
          * This is a 'fancy' capability that should be used with caution.
          *
          * @param event   The event that is to be handled.
          * @param handler The handler function object to replace the existing one.
          * @return The event handler that was replaced.
          */
         virtual EventHandler ReplaceEventHandler(
            const Event & event,
            const EventHandler & handler
            )
         {
            EventHandler old_handler{};
            auto sit = single_dispatch.find( event );
            if ( sit != single_dispatch.end() )
            {
               if ( sit->second != nullptr )
               {
                  std::swap( sit->second, old_handler );
                  single_dispatch[event] = handler;
                  return std::move( old_handler );
               }
            }
            auto dit = dispatch_table.find( event );
            if ( dit != dispatch_table.end() )
            {
               std::swap( dit->second, old_handler );
               dispatch_table[event] = handler;
            }
            return std::move( old_handler );
         }

         /**
          * Designates an event to be stored and forwarded to the next state instead
          * of handling it or performing a state transition based on it. This is useful
          * when an event arrives 'early' in some state, but it is actually useful in a
          * subsequent state and loss of the event makes things difficult. The usefulness
          * of this capability is limited by whether the event so stored can be handled
          * out of order.
          *
          * @param event   The event that is to be stored and forwarded to the next state.
          * @param eset    The state transition events that forward the event.
          * @return The reference to this State object.
          */
         virtual State & StoreAndForwardEvent(
            const Event & event,
            const EventSet & eset
            )
         {
            forwarded_events.insert( event );
            for ( auto eit : eset )
            {
               auto sfit = storeforward_table.find( eit );
               if ( sfit != storeforward_table.end() )
               {
                  sfit->second.insert( event );
               }
               else
               {
                  EventSet eset{ event };
                  storeforward_table[eit] = eset;
               }
            }
            return *this;
         }

         /**
          * Called by the FiniteStateMachine class when it is dispatching an
          * event to the state. The state returns the corresponding event
          * handler which is subsequently called to handle the event.
          * Note that the state object is simply a container for these functions.
          * All activity takes place in the event handler (not the State) in the
          * context of the FiniteStateMachine's DeliverNextEvent() method.
          *
          * @return The EventHandler for the specified event or nullptr if no such.
          */
         inline EventHandler HandlerForEvent(
            const Event & event
            ) const
         {
            EventHandler handler{ default_handler };
            auto dit = dispatch_table.find( event );
            if ( dit != dispatch_table.end() )
            {
               handler = dit->second;
            }
            auto sit = single_dispatch.find( event );
            if ( sit != single_dispatch.end() )
            {
               if ( sit->second == nullptr )
               {
                  sit->second = dit->second;
                  dit->second = [this]( const Event & ) { /* NOOP */ };
               }
            }
            return std::move( handler );
         }


         /**
          * Gets the state specific default event handler function.
          *
          * @return The default handler function object.
          */
         virtual EventHandler DefaultEventHandler()
         {
            return default_handler;
         }


         /**
          * Called by the FiniteStateMachine when a state transition is
          * taking place and the state is becoming the new current state.
          *
          * @return The EntryFunction of the state.
          */
         inline EntryFunction Entry() const { return entry_function; }


         /**
          * Called by the FinitieStateMachine when a state transition is
          * taking place and the state is becoming the previous state.
          * @return The ExitFunction of the state.
          */
         inline ExitFunction Exit() const { RestoreDispatchTable(); return exit_function; }


         /**
          * Returns whether the state is equipped with 'store-and-forward' events.
          *
          * @return true if the state supports store-and-forward events.
          */
         inline bool IsStoringForwarding() const
         {
            return ! forwarded_events.empty();
         }


         /**
          * Stores the given event in the event store.
          *
          * @param event   The event to store (for forwarding).
          */
         inline void StoreEvent(
            const Event & event
            )
         {
            event_store.push_back( event );
         }


         /**
          * Clears the event store of the state.
          */
         inline void ClearEventStore() { event_store.clear(); }


         /**
          * Checks whether the event store of the state is empty.
          */
         inline bool IsEventStoreEmpty() { return event_store.empty(); }


         /**
          * Forwards events that were stored based on the transition event given.
          *
          * @param event   The transition event.
          * @param destination   The next state's event_store.
          */
         void ForwardEvents(
            const Event & event,
            Events & destination
            )
         {
            auto sfit = storeforward_table.find( event );
            if ( sfit != storeforward_table.end() )
            {
               const EventSet & eset( sfit->second );
               for ( auto sit : event_store )
               {
                  if ( eset.find( sit ) != eset.end() )
                  {
                     destination.push_back( sit );
                  }
               }
            }
            event_store.clear();
         }


      private :
         /// The mechanism used to associate an event handler with an event in
         /// the state.
         ///
         using DispatchTable = std::map< Event, EventHandler >;

         /// Those events that should be handled only once in this state
         /// are entered here. Just the fact that an entry exists in this
         /// table assures that event is a single dispatch event. On exit
         /// the event dispatch is restored.
         ///
         using SingleDispatch = std::map< Event, EventHandler >;

         /// Describes what state transition to take when an event arrives for
         /// the state. Note that the DispatchTable and the TransitionTable
         /// are mutually exclusive (the same event can't be in both).
         ///
         using TransitionTable = std::map< Event, StateID >;

         /// The events that are being forwarded in this state.
         ///
         using ForwardedEvents = EventSet;

         /// Each event transition (key) passes the events in the set. If the set
         /// is empty then all stored events are passed. If there is no entry in the
         /// table for the (transition) event then all stored events are discarded.
         ///
         using StoreAndForwardTable = std::map< Event, EventSet >;


      private :
         EntryFunction             entry_function;
         ExitFunction              exit_function;
         mutable DispatchTable     dispatch_table;
         mutable SingleDispatch    single_dispatch;
         EventHandler              default_handler;  // state specific default handler
         TransitionTable           transition_table;
         ForwardedEvents           forwarded_events;
         Events                    event_store;
         StoreAndForwardTable      storeforward_table;


      private :
         /**
          * For those events that are to be dispatched only once it restores
          * the dispatch table so that it may handle the event again.
          */
         void RestoreDispatchTable() const
         {
            for ( auto sit : single_dispatch )
            {
               if ( sit.second != nullptr )
               {
                  dispatch_table[sit.first] = sit.second;
                  sit.second = nullptr;
               }
            }
         }
      };


   public :
      /// A value that may be returned when requesting the current state and
      /// the initial state has not yet been set.
      ///
      static const StateID SentinelStateID{ -1 };

      /// Convenience value to descriptively set the similarly named function
      /// argument in SetEventHandler().
      ///
      static const bool SingleDispatchOnly{ true };

   public :
      /**
       * Default constructor.
       */
      FiniteStateMachine()
         :  previous_state( SentinelStateID )
         ,  current_state( SentinelStateID )
      { }


      /**
       * The destructor.
       */
      virtual ~FiniteStateMachine() = default;


      /**
       * Registers the passed in state object with the FSM. The state is 
       * associated with an identifier that is static for the life of the FSM.
       * The passed in state object remains the property and responsibility of
       * the caller.
       *
       * @param state   The state object to register with the FSM.
       * @return the state identifier chosen for the state.
       */
      virtual StateID RegisterState(
         State *state
         )
      {
         int state_number = static_cast<int>( state_table.size() );
         state_table.push_back( state );
         return state_number;
      }


      /**
       * The primary work horse method of the FSM which posts (enqueues) the
       * provided event on the normal priority event queue.
       *
       * @param event The event that is to be queued for execution by the state
       *              machine.
       */
      inline void PostEvent(
         const Event & event
         )
      {
         EventDeliveryMethod( event );
      }


      /**
       * The secondary work horse method of the FSM when it is necessary to
       * post a high priority event (internal event) that should be executed before
       * any of the normal priority events. Enables the FSM to make multiple
       * state transitions between normal priority events (thus the term internal).
       *
       * @param event The internal event that is to be queued for execution by the
       *              state machine.
       */
      inline void PostInternalEvent(
         const Event & event
         )
      {
         internal_events.push_back( event );
      }


      /**
       * @return the current state identifier of the FSM.
       */
      inline StateID CurrentState() const
      {
         return current_state;
      }


      /**
       * @return the previous state identifier of the FSM.
       */
      inline StateID PreviousState() const
      {
         return previous_state;
      }


      /**
       * Allows the outright setting of the current state w/o executing any
       * events or entry/exit functions.
       *
       * @param new_state  The new state that should become the FSM's current state.
       */
      void SetInitialState(
         StateID new_state
         )
      {
         if ( new_state > (StateID) state_table.size() || new_state < LowestValidStateID )
         {
            throw Exception("Out of bounds state id number.");
         }

         if ( state_table[current_state]->IsStoringForwarding() )
         {
            // Clear any stored events since this method essentially acts as
            // a state machine reset function.
            //
            state_table[current_state]->ClearEventStore();
         }

         current_state = new_state;
      }


   protected :
      /// The states registered for this state machine are held in this type.
      /// The raw pointer here is used only as a reference and the FSM must
      /// assure that the lifetime of States are longer than the state table.
      ///
      using StateTable = std::vector< State * >;


   protected :
      /// A boundary number that identifies the lowest numeric value for a
      /// a state number.
      ///
      static const StateID LowestValidStateID{ 0 };


   protected :

      /**
       * The method that actually performs the event delivery. It is meant to
       * help with the asynchronous version of this class. In that case a
       * proper replacement should be defined that delivers the event to the
       * appropriate thread context.
       */
      virtual void EventDeliveryMethod( const Event & event )
      {
         events.push_back( event );
         while ( DeliverNextEvent() );
      }


      /**
       * The handler installed here catches any events that the current
       * state does not handle. If not set, undefined events for a particular
       * state are simply ignored. If defined, then it is executed if the
       * state has no event handler defined.
       */
      inline void InstallDefaultEventHandler( const EventHandler & handler )
      {
         default_handler = handler;
      }


      /**
       * @return true if there are events waiting to be executed.
       */
      inline bool HaveEventsToProcess() const
      {
         return ! (internal_events.empty() && events.empty());
      }


      /**
       * The driving function that consumes and executes the queued up events.
       * Priority is given to 'internal' events. Only a single event at a time
       * is executed.
       * In a synchronous FSM the method should be called from 'interface'
       * methods that are called by foreign objects only (just after the
       * corresponding event is posted - see examples).
       *
       * @return true if there are more events available to process.
       */
      bool DeliverNextEvent()
      {
         // Indicates that the intial state has not been set via the
         // SetInitialState() method.
         //
         assert( current_state != SentinelStateID );

         if ( ! HaveEventsToProcess() )
         {
            return false;
         }

         if ( ! internal_events.empty() )
         {
            Event ev( internal_events.front() );
            State *state = state_table[current_state];

            assert( state != nullptr );

            StateID target = state->TransitionForEvent( ev );
            if ( target != SentinelStateID )
            {
               TransitionToState( target, ev );
            }
            else
            {
               EventHandler handler = state->HandlerForEvent( ev );
               if ( handler != nullptr )
                  handler( ev );
               else if ( default_handler != nullptr )
                  default_handler( ev );
            }
            internal_events.pop_front();
         }
         else if ( ! events.empty() )
         {
            Event ev( events.front() );
            State *state = state_table[current_state];

            assert( state != nullptr );

            StateID target = state->TransitionForEvent( ev );
            if ( target != SentinelStateID )
            {
               TransitionToState( target, ev );
            }
            else
            {
               EventHandler handler( state->HandlerForEvent( ev ) );
               if ( handler != nullptr )
                  handler( ev );
               else if ( default_handler != nullptr )
                  default_handler( ev );
            }
            events.pop_front();
         }

         return HaveEventsToProcess();
      }


      /**
       * @return the number of events queued for execution in the external event queue.
       */
      inline size_t ExternalEventBacklog() const
      {
         return events.size();
      }


      /**
       * @return the number of events queued for execution in the internal event queue.
       */
      inline size_t InternalEventBacklog() const
      {
         return internal_events.size();
      }


      /**
       * Remove and discard all events from the external event queue.
       */
      inline void PurgeExternalEvents()
      {
         events.clear();
      }


      /**
       * Remove and discard all events from the internal event queue.
       */
      inline void PurgeInternalEvents()
      {
         internal_events.clear();
      }


      /**
       * Removes the specific event from the external event queue.
       */
      inline void RemoveEvent( const Event & event )
      {
         auto it = std::find( events.begin(), events.end(), (int) event );
         if ( it != events.end() ) { events.erase( it ); }
      }


      /**
       * Removes the specific event from the external event queue.
       */
      inline void RemoveInternalEvent( const Event & event )
      {
         auto it = std::find( internal_events.begin(), internal_events.end(), (int) event );
         if ( it != internal_events.end() ) { internal_events.erase( it ); }
      }


   private :
      StateID        previous_state;   /// The state prior to the current one.
      StateID        current_state;    /// The index into the state table.
      StateTable     state_table;      /// The registered states.
      Events         events;           /// External events queued up.
      Events         internal_events;  /// Internal events queued up.

      /// Set to handle the case when the current state has no suitable event
      /// handler
      ///
      EventHandler   default_handler;


   private :
      /**
       * The mechanism for performing state transitions. If there is an existing
       * non-null state then it's exit function is called (if not null). Once
       * the new state is set the entry function of the new state is called (if
       * not null).
       *
       * @param new_state  The state to which the FSM should transition to.
       * @param event      The event that caused the state transition.
       *
       * @return the previous state (from which the transition was taken).
       */
      StateID TransitionToState(
         StateID new_state,
         const Event & event
         )
      {
         // Indicates that the intial state has not been set via the
         // SetInitialState() method.
         //
         assert( current_state != SentinelStateID );

         if ( new_state == current_state )
         {
            return current_state;   // no transition necessary
         }

         if ( new_state > (StateID) state_table.size() || new_state < LowestValidStateID )
         {
            std::ostringstream ostr;
            ostr  << "Out of bounds state id number '" << new_state
                  << "' on transition from state '" << current_state
                  << "' via event '" << (int)event << "'.";
            throw Exception( ostr.str() );
         }

         ExitFunction on_exit( state_table[current_state]->Exit() );
         if ( on_exit != nullptr )
         {
            on_exit( event );
         }

         if ( state_table[current_state]->IsStoringForwarding() )
         {
            // Any stored events are now forwarded as if they arrived as internal
            // (high priority) events.
            //
            assert( state_table[new_state]->IsEventStoreEmpty() );
            state_table[current_state]->ForwardEvents( event, internal_events );
         }

         previous_state = current_state;
         current_state = new_state;

         EntryFunction on_entry( state_table[current_state]->Entry() );
         if ( on_entry != nullptr )
         {
            on_entry( event );
         }

         return previous_state;
      }

   };

}

