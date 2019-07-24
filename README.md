# ProtEnc

**Prot**ocol**Enc**oder is a C++ typestate library, making it easy to enforce
a protocol on the usage of an object.

## What are typestates?

[Typestate analysis](https://en.wikipedia.org/wiki/Typestate_analysis) is a way
to specify at compile-time what operations are valid on an object in the
current state, and enforce these restrictions.

In other words, it allows to define a protocol of how to use an object, and
make sure that the program cannot breach this protocol.

### Example: HTTP connection builder

HTTP connections have headers and a body (among other things). We want to
create a builder for one such connection, making sure that:
  - There is at least one header.
  - There is a body.
  - The headers are specified before the body.

Only then will the user be able to create a connection.

_(Bear in mind, this is a simplistic example, which could be better solved with
a constructor taking the parameters.)_

Let's create a class to build this connection:

```C++
class HTTPConnectionBuilder {
 public:
  void add_header(std::string header) { ... }
  void add_body(std::string body) { ... }
  HTTPConnection build() { ... }
 private:
  ...
};
```

Now, we need to enforce the protocol. We can represent it with the following
graph:

![HTTP connection builder protocol
graph](https://github.com/nitnelave/ProtEnc/blob/master/images/http_connection.jpg)

We start in the "START" state, then we have to take the transition `add_header`
at least once, but as many more times as we want, then the `add_body`
transition, then we can `build` the connection.

After using ProtEnc, we get a wrapper object that we can use like this:

```c++
  HTTPConnection connection = GetConnectionBuilder()
                                .add_header("First header")
                                .add_header("Second header")
                                .add_body("Body")
                                .build();
```

Any digression from the protocol will result in a compilation error.

#### Defining the protocol

To define the protocol, we need to build the graph shown above. Let's start by
declaring the possible states:

```c++
enum class HTTPBuilderState {
  START,
  HEADERS,
  BODY
};
```

Now we can declare from which states it is valid to start; in our case, only
START:

```c++
using MyInitialStates = InitialStates<HTTPBuilderState::START>;
```

Similarly, we can declare that we can finish only from the BODY state, by
calling the `build` function:

```c++
using MyFinalStates = FinalStates<
    FinalState<HTTPBuilderState::BODY, &HTTPConnectionBuilder::build>
  >;
```

Let's declare the transitions: we can go from START to HEADERS by calling
`add_header`. It looks like this:

```
Transition<HTTPBuilderState::START, HTTPBuilderState::Header,
           &HTTPConnectionBuilder::add_header>
```

We can add the other transitions in the same way, giving us:

```
using MyTransitions = Transitions<
      // START -> HEADERS with add_header
      Transition<HTTPBuilderState::START, HTTPBuilderState::HEADERS,
                 &HTTPConnectionBuilder::add_header>,
      // HEADERS -> HEADERS with add_header
      Transition<HTTPBuilderState::HEADERS, HTTPBuilderState::HEADERS,
                 &HTTPConnectionBuilder::add_header>,
      // HEADERS -> BODY with add_body
      Transition<HTTPBuilderState::HEADERS, HTTPBuilderState::BODY,
                 &HTTPConnectionBuilder::add_body>
  >;
```

Then we just have to declare our wrapper class with all this info:

```
PROTENC_START_WRAPPER(
    // Wrapper name
    HTTPConnectionBuilderWrapper,
    // Implementation class
    HTTPConnectionBuilder,
    // Name of the enum
    HTTPBuilderState,
    // Initial states
    MyInitialStates,
    // Transitions
    MyTransitions,
    // Final states
    MyFinalStates,
    // List of valid query methods (const methods on the implementation)
    MyValidQueries);

  // Declare the list of functions that we are wrapping:
  // Transitions
  PROTENC_DECLARE_TRANSITION(add_header);
  PROTENC_DECLARE_TRANSITION(add_body);

  // End functions.
  PROTENC_DECLARE_FINAL_STATE(build);

PROTENC_END_WRAPPER;
```

Now we can create an instance of our wrapper with any of the starting states:

```
HTTPConnection connection =
  HTTPConnectionBuilderWrapper<HTTPBuilderState::Start>{}
      .add_header("Never")
      .add_header("Gonna")
      .add_body("Give you up")
      .build();
```

## What can this library be used for?

Any protocol that can be represented by a FSM can be encoded using this
library. This provides compile-time guarantee that the code respects the
protocol.

This is a step towards formal verification of code, while being more
lightweight and easy to integrate into existing codebases.

The definition of the wrapper could be outsourced to a code generator that
checks some properties on the FSM, or simply takes an existing protocol
specification and turn the FSM into a wrapper.

## But how does it work?

Oh, you want to get into the details? TL;DR: lots of template magic :)

### The wrapper object

The wrapper object is templated by the enum representing the states. The value
of the template parameter is the state in which the wrapper is. Every
transition method (including the final state methods) consume the object
(taking it by r-value reference), and return a new wrapper in the new state.
The underlying object is never copied (provided it has a good move
constructor).

The wrapper mainly just forwards the calls (declared by `DECLARE_TRANSITION`
and friends) to the `GenericWrapper`.

### The `GenericWrapper` object

`GenericWrapper` does the heavy lifting:
  - It contains an instance of the wrapped class (by value, there is no
    indirection).
  - It is templated by all the parameters describing the FSM: initial states,
    transitions and so on.
  - It has a `call_transition` method (and friends) that takes a function
    pointer, checks that it's a valid transition, and returns the new state.
  - It has a bunch of checks to make sure that we have "pretty" error messages
    for common errors (not using the correct types to define the FSM, not
    consuming the object for a final state transition, etc).

### `call_transition`

`call_transition` is called with a pointer to the function you want to call
(and arguments). It has a `static_assert` that will go over the list of
transitions, looking for one that starts from the current state, and has the
function pointer as label. If the transition is not found, it's a compile
error. If the transition is found, we can extract from it the target state. We
then call the function on the wrapped object, and return a new wrapper
(constructed by moving the current wrapper) in the new state.

The other functions (`call_final_state`, `call_valid_query`) work in a similar
fashion, deducing the return type from the function pointer and the arguments.

The GenericWrapper constructor checks that it was built with one of the initial
states (with a `static_assert`).

### The nitty-gritty of the magic

If you want to get into the template details, I invite you to have a look at
the code itself.


## Caveats

- **It requires C++20.** This is mainly due to the `auto` template parameters.
  The omitted return types could be fixed with some redundancy (decltype), but
  dropping the `auto` template parameters would mean dropping support for enum
  classes. It could still be acceptable, and regular enums mapped to ints would
  also work (it would just be slightly less elegant). The code could be adapted
  to work with C++14 if needed (maybe even C++11, I'm not sure), but it would
  be more verbose.

## Potential improvements

- **We could have more checks.**
  - We currently only have basic checks on the
    template parameters, but we could check that there is no 2 transitions with
    the same origin and label, or that there are no unreachable states. I feel
    that this is more the responsability of the user: they need to make sure the
    FSM is the right one for their protocol.
  - We check the top-level list parameters (`Transitions`, `FinalStates`, ...)
    but we could go down and check that the function pointers are from the
    wrapped class, that the types are `Transition`/`FinalState`/..., that the
    states are from the right enum, and so on.
- **Better error messages.** When the transition is not found, we have static
  information about the state and the function pointer, so it should
  theoretically be possible to craft an error message with more information
  than "Transition not found".
