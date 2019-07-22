#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "protenc.h"

// Example use of the ProtEnc library: an advanced builder pattern.
//
// Here, we have an HTTP connection builder. We want to force the user to add
// one or more headers, then exactly one body, after which he will be able to
// start the connection. ProtEnc will enforce that our object is in the right
// state at every step.
//
// We can represent those constraints as a Finite State Machine (FSM)
// (https://en.wikipedia.org/wiki/Finite-state_machine), which is equivalent to
// a regex. In other words, our protocol looks like the regex:
//   (header)+(body)(start)
//
// If we turn that into a FSM, we get:
//
//  *******               *********             ******
//  *START* ---header---> *HEADERS* ---body---> *BODY* -->start<--
//  *******               *********             ******
//                         |    ^
//                         |    |
//                         header
//
// We start in the state "START", from there we can add a header, which leads
// us to the state "HEADERS". There, we have a choice: we can keep adding
// headers as many times as we want, always looping back to the same state, or
// we can add a body, getting us to the state "BODY". From there, we just have
// to call "start", which is the final transition into an accepting state.
//
// The setup consists of:
//   - An enum to list the states.
//   - The types describing our state machine (initial states, final states,
//     transitions, ...).
//   - The implementation of the builder, unconstrained.
//   - The wrapper class, which will do the constraining.


// Dummy connection class.
using HTTPConnection = std::tuple<std::vector<std::string>, std::string>;


// List of states in our FSM.
enum class HTTPStarterState {
  // Empty class.
  START,
  // Added at least one header.
  HEADERS,
  // Added the body.
  BODY
};

// Forward-declaration of the wrapper, to make it a friend of
// HTTPConnectionStarter. That way, we can hide the constructor of
// HTTPConnectionStarter, preventing users from creating an unwrapped
// HTTPConnectionStarter.
template <HTTPStarterState>
class HTTPConnectionStarterWrapper;

// Basic implementation of the class, without the protocol constraints.
class HTTPConnectionStarter {
 public:

  void add_header(std::string header) {
    headers_.emplace_back(std::move(header));
  }

  void add_body(std::string body) {
      body_ = std::move(body);
  }

  // This is a query function: it will just return information, without
  // changing the object.
  size_t num_headers() const {
      return headers_.size();
  }

  // Start the connection. This should consume the object.
  HTTPConnection
  start() && {
    return HTTPConnection(std::move(headers_), std::move(body_));
  }

 private:
  // Constructor is private, only the wrapper gets to build one, so that
  // no one builds a connection starter not wrapped.
  HTTPConnectionStarter() = default;

  // Only the wrapper has access to the class, to build it.
  template <HTTPStarterState>
  friend class ::HTTPConnectionStarterWrapper;

  // Internal fields.
  std::vector<std::string> headers_;
  std::string body_;
};

using prot_enc::Transitions;
using prot_enc::Transition;
using prot_enc::FinalStates;
using prot_enc::FinalState;
using prot_enc::ValidQueries;
using prot_enc::ValidQuery;
using prot_enc::InitialStates;

// Definition of the graph, from the initial states, transitions and final
// states.
// The use of aliases allows us to get around the limitations of macros with
// parameters containing commas.

// Initial states (can be a list).
using MyInitialStates = InitialStates<HTTPStarterState::START>;

// Transitions, of the form <starting state, end state, function pointer>.
using MyTransitions = Transitions<
      // We can go from START to HEADERS by calling add_header.
      Transition<HTTPStarterState::START, HTTPStarterState::HEADERS,
                 &HTTPConnectionStarter::add_header>,
      // This is the loop: we stay in the state HEADERS.
      Transition<HTTPStarterState::HEADERS, HTTPStarterState::HEADERS,
                 &HTTPConnectionStarter::add_header>,
      Transition<HTTPStarterState::HEADERS, HTTPStarterState::BODY,
                 &HTTPConnectionStarter::add_body>
  >;

// Accepting states, of the form <accepting state, end function pointer>.
using MyFinalStates = FinalStates<
   FinalState<HTTPStarterState::BODY, &HTTPConnectionStarter::start>
  >;

// Valid information queries, of the form <accepting state, end function
// pointer>.
using MyValidQueries = ValidQueries<
   // We can only call num_headers from the state BODY.
   ValidQuery<HTTPStarterState::BODY, &HTTPConnectionStarter::num_headers>
  >;

// This is the declaration of the wrapper: it is a class declaration.
PROTENC_START_WRAPPER(HTTPConnectionStarterWrapper, HTTPConnectionStarter,
                      HTTPStarterState, MyInitialStates, MyTransitions,
                      MyFinalStates, MyValidQueries);

  // Declare the list of functions that we are wrapping:
  // Transitions
  PROTENC_DECLARE_TRANSITION(add_header);
  PROTENC_DECLARE_TRANSITION(add_body);

  // End functions.
  PROTENC_DECLARE_FINAL_STATE(start);

  // Query functions.
  PROTENC_DECLARE_QUERY_METHOD(num_headers);

PROTENC_END_WRAPPER;

// Factory method. Note that trying to build a wrapper in any other state than
// START (because it's in our initial state list) will fail.
static HTTPConnectionStarterWrapper<HTTPStarterState::START>
GetConnectionStarter() {
  return {};
}

int main() {
    // Get the connection with an easy interface.
    // The order of the functions is checked at compile-time by the state
    // markers.
    HTTPConnection connection_1 = GetConnectionStarter()
                                  .add_header("First header")
                                  .add_header("Second header")
                                  .add_body("Body")
                                  .start();

    // Print the body of the connection, just to check that we succeeded.
    std::cout << std::get<1>(connection_1) << '\n';

    // The query is only valid once we have the body.
    auto connection_part = GetConnectionStarter()
                           .add_header("First header")
                           .add_body("Body");
    std::cout << "Num headers: " << connection_part.num_headers() << '\n';
    // We have to move the connection_part, because all the operations consume
    // the object and return a new one.
    auto connection_2 = std::move(connection_part).start();

    // This doesn't compile:
    // GetConnectionStarter().add_body("Body");
    // GetConnectionStarter().add_header("First header")
    //                       .add_body("Body")
    //                       .add_header("Second header");
    // GetConnectionStarter().add_header("Header").start();

    return 0;
}
