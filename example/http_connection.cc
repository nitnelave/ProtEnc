#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "protenc.h"


// Dummy connection class
using HTTPConnection = std::tuple<std::vector<std::string>, std::string>;


// Marker class for the state.
enum class HTTPStarterState {
  // Empty class.
  START,
  // Added at least one header.
  HEADERS,
  // Added the body.
  BODY
};

// Forward-declaration of the wrapper, to make it a friend of
// HTTPConnectionStarter.
template <HTTPStarterState>
class HTTPConnectionStarterWrapper;

// Basic implementation of the class, without the protocol constraints.
// We want to enforce that the user adds one or more headers, then a body, then
// start the connection.
class HTTPConnectionStarter {
 public:

  void add_header(std::string header) {
    headers_.emplace_back(std::move(header));
  }

  void add_body(std::string body) {
      body_ = std::move(body);
  }

  size_t num_headers() const {
      return headers_.size();
  }

  // Start the connection
  HTTPConnection
  start() {
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

// Definition of the graph:

// Initial states (can be a list).
using MyInitialStates = InitialStates<HTTPStarterState::START>;

// Transitions, of the form <starting state, end state, function pointer>.
using MyTransitions = Transitions<
      Transition<HTTPStarterState::START, HTTPStarterState::HEADERS,
                 &HTTPConnectionStarter::add_header>,
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
   ValidQuery<HTTPStarterState::BODY, &HTTPConnectionStarter::num_headers>
  >;

PROTENC_START_WRAPPER(HTTPConnectionStarterWrapper, HTTPConnectionStarter,
                      HTTPStarterState, MyInitialStates, MyTransitions,
                      MyFinalStates, MyValidQueries);

  PROTENC_DECLARE_TRANSITION(add_header);
  PROTENC_DECLARE_TRANSITION(add_body);

  PROTENC_DECLARE_FINAL_STATE(start);

  PROTENC_DECLARE_QUERY_METHOD(num_headers);

PROTENC_END_WRAPPER;

static HTTPConnectionStarterWrapper<HTTPStarterState::START>
GetConnectionStarter() {
  return {};
}

int main() {
    // Get the connection with an easy interface.
    // The order of the functions is checked at compile-time by the state
    // markers.
    auto connection_1 = GetConnectionStarter()
                        .add_header("First header")
                        .add_header("Second header")
                        .add_body("Body")
                        .start();

    std::cout << std::get<1>(connection_1) << '\n';

    // The query is only valid once we have the body.
    auto connection_part = GetConnectionStarter()
                           .add_header("First header")
                           .add_body("Body");
    std::cout << "Num headers: " << connection_part.num_headers() << '\n';
    auto connection_2 = std::move(connection_part).start();

    return 0;
}
