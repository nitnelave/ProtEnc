/**********************************
 * MIT License (see LICENSE).     *
 *                                *
 * Copyright (c) 2019 nitnelave   *
 *                                *
 **********************************
 *
 *
 * ProtEnc -- Protocol Encoder
 *
 * This is a typestate library for C++. It enables you to easily create objects
 * that are associated with a state, and functions restricted to each state.
 * See the README.md for more on typestates and what they are used for.
 **/

#include <utility>

namespace prot_enc {


/******************************************************************************
 *  PUBLICLY VISIBLE TYPES                                                    *
 ******************************************************************************/


// Transitions, initial/final states and valid queries, to encode our FSM.
template <auto StartState, auto EndState, auto FunctionPointer>
struct Transition;
template <auto StartState, auto FunctionPointer>
struct FinalState;
template <auto StartState, auto FunctionPointer>
struct ValidQuery;

template <auto... InitialState>
struct InitialStates;

template <typename... Transition>
struct Transitions;

template <typename... FinalState>
struct FinalStates;

template <typename... ValidQuery>
struct ValidQueries;

// These macros are just here so that we can end any macro with a semicolon.
#define PROTENC_MACRO_END_2(LINE) struct some_improbable_long_function_name ## LINE {}
#define PROTENC_MACRO_END_1(LINE) PROTENC_MACRO_END_2(LINE)
#define PROTENC_MACRO_END PROTENC_MACRO_END_1(__LINE__)

// Macros to facilitate the declaration of wrappers. The structure is:
//
// START_WRAPPER_DECL(args...);
//   DECLARE_TRANSITION(function_name);
//   DECLARE_TRANSITION(function_name2);
//   DECLARE_FINAL_STATE(function_name3);
//   DECLARE_QUERY_METHOD(function_name4);
//   ...
// END_WRAPPER_DECL;
//
// The arguments are:
//   - WRAPPER_TYPE: The name of the wrapper class we are declaring.
//   - WRAPPED_TYPE: The name of the class containing the implementation of the
//     functions
//   - STATE_TYPE: The type of the enum-like values used to differentiate the
//     states. Enum class are recommended, but anything that can be a template
//     parameter value is okay.
//   - INITIAL_STATES: The type containing the list of initial states. It should
//     be of the form: "InitialStates<state_value_1, state_value_2, ...>".
//   - TRANSITIONS: The type containing the valid transitions. It should be of
//     the form:
//     "Transitions<
//        Transition<start_state, end_state,
//                   &WRAPPED_TYPE::my_function>,
//        ...
//      >"
//   - FINAL_STATES: The type containing the valid end states. It should be of
//     the form:
//     "FinalStates<
//          FinalState<start_state, &WRAPPED_TYPE::function_name>,
//          ...
//      >"
//   - VALID_QUERIES: The type containing the valid query functions. It should
//     be of the form:
//     "ValidQueries<
//          ValidQuery<start_state, &WRAPPED_TYPE::function_name>,
//          ...
//      >"

#define PROTENC_START_WRAPPER(WRAPPER_TYPE, WRAPPED_TYPE, STATE_TYPE,          \
                              INITIAL_STATES, TRANSITIONS, FINAL_STATES,       \
                              VALID_QUERIES)                                   \
  template<STATE_TYPE CurrentState>                                            \
  class WRAPPER_TYPE                                                           \
    : public ::prot_enc::internal::GenericWrapper<                             \
          CurrentState, WRAPPER_TYPE, WRAPPED_TYPE, INITIAL_STATES,            \
          TRANSITIONS, FINAL_STATES, VALID_QUERIES>                            \
  {                                                                            \
   public:                                                                     \
    using Base =                                                               \
        ::prot_enc::internal::GenericWrapper<                                  \
            CurrentState, WRAPPER_TYPE,  WRAPPED_TYPE, INITIAL_STATES,         \
            TRANSITIONS, FINAL_STATES, VALID_QUERIES>;                         \
    using Wrapped = WRAPPED_TYPE;                                              \
   private:                                                                    \
    /* Allow the GenericWrapper to construct an instance of this with a */     \
    /* different state. */                                                     \
    template<auto, template <auto> typename, typename, typename, typename,     \
             typename, typename>                                               \
    friend class ::prot_enc::internal::GenericWrapper;                         \
    WRAPPER_TYPE(Base&& other) : Base(other) {}                                \
   public:                                                                     \
    WRAPPER_TYPE() : Base(Wrapped{}) {}                                        \
    PROTENC_MACRO_END


#define PROTENC_DECLARE_TRANSITION(name)                      \
    template <typename... Args>                               \
    auto name(Args&&... args) && {                            \
      return std::move(*this)                                 \
          .template call_transition<&Wrapped::name, Args...>( \
              std::forward<Args>(args)...);                   \
    } PROTENC_MACRO_END


#define PROTENC_DECLARE_FINAL_STATE(name)                      \
    template <typename... Args>                                \
    auto name(Args&&... args) && {                             \
      return std::move(*this)                                  \
          .template call_final_state<&Wrapped::name, Args...>( \
              std::forward<Args>(args)...);                    \
    } PROTENC_MACRO_END

#define PROTENC_DECLARE_QUERY_METHOD(name)                             \
    template <typename... Args>                                        \
    auto name(Args&&... args) const {                                  \
      return this->template call_valid_query<&Wrapped::name, Args...>( \
          std::forward<Args>(args)...);                                \
    } PROTENC_MACRO_END

#define PROTENC_END_WRAPPER }




/******************************************************************************
 *  IMPLEMENTATION DETAILS                                                    *
 ******************************************************************************/
namespace internal {

// Get the target state of a transition, if it exists (for this starting state
// and function pointer).
template <auto CurrentState, auto FunctionPointer, typename... Transitions>
struct find_transition_t {
  struct Transition;
  struct Found;
  static_assert(std::is_same<Transition, Found>::value, "Transition not found");
};

// Found the transition.
template <auto TransitionEnd, auto CurrentState, auto FunctionPointer,
          typename... Transitions>
struct find_transition_t<CurrentState, FunctionPointer,
                         Transition<CurrentState, TransitionEnd,
                                    FunctionPointer>,
                         Transitions...> {
  using type = Transition<CurrentState, TransitionEnd, FunctionPointer>;
};

// Found the end state.
template <auto CurrentState, auto FunctionPointer, typename... Transitions>
struct find_transition_t<CurrentState, FunctionPointer,
                         FinalState<CurrentState, FunctionPointer>,
                         Transitions...> {
  using type = FinalState<CurrentState, FunctionPointer>;
};

// Found the valid query.
template <auto CurrentState, auto FunctionPointer, typename... Transitions>
struct find_transition_t<CurrentState, FunctionPointer,
                         ValidQuery<CurrentState, FunctionPointer>,
                         Transitions...> {
  using type = ValidQuery<CurrentState, FunctionPointer>;
};

// Recurse.
template <auto CurrentState, auto FunctionPointer, typename Transition,
          typename... Transitions>
struct find_transition_t<CurrentState, FunctionPointer, Transition,
                         Transitions...> {
  using type = typename find_transition_t<CurrentState, FunctionPointer,
                                          Transitions...>::type;
};

// Unwrap FinalStates.
template <auto CurrentState, auto FunctionPointer, typename... FinalState>
struct find_transition_t<CurrentState, FunctionPointer,
                         FinalStates<FinalState...>> {
  using type = typename find_transition_t<CurrentState, FunctionPointer,
                                          FinalState...>::type;
};

// Unwrap Transitions.
template <auto CurrentState, auto FunctionPointer, typename... Transition>
struct find_transition_t<CurrentState, FunctionPointer,
                         Transitions<Transition...>> {
  using type = typename find_transition_t<CurrentState, FunctionPointer,
                                          Transition...>::type;
};


// Unwrap ValidQuery.
template <auto CurrentState, auto FunctionPointer, typename... ValidQuery>
struct find_transition_t<CurrentState, FunctionPointer,
                         ValidQueries<ValidQuery...>> {
  using type = typename find_transition_t<CurrentState, FunctionPointer,
                                          ValidQuery...>::type;
};

// Find the transition (if any) starting from CurrentState, with label
// FunctionPointer. This can return either a Transition, a FinalState or a
// ValidQuery.
template <auto CurrentState, auto FunctionPointer, typename... Transitions>
using find_transition =
    typename find_transition_t<CurrentState, FunctionPointer, Transitions...>
        ::type;


// Get the target state of a transition, if it exists (for this starting state
// and function pointer).
template <typename T>
struct return_of_transition_t {
  struct Transition;
  struct Found;
  static_assert(std::is_same<Transition, Found>::value, "Transition not found");
};

// The transition is a transition (and not a final state or query function).
template <auto TransitionEnd, auto CurrentState, auto FunctionPointer>
struct return_of_transition_t<Transition<CurrentState, TransitionEnd,
                              FunctionPointer>> {
  static constexpr auto EndState = TransitionEnd;
};

template <typename Transitions, auto CurrentState, auto FunctionPointer>
constexpr auto return_of_transition =
    return_of_transition_t<
        find_transition<CurrentState, FunctionPointer, Transitions>>::EndState;


// Get the result of an end function, if it is valid (i.e. we have an end state
// declared for this state and function pointer).
template <typename T, typename... Args>
struct return_of_final_state_t{
  struct FinalState;
  struct Found;
  static_assert(std::is_same<FinalState, Found>::value,
                "Final state not found");
};

// Found the final state. Needs the arguments for result_of.
template <auto CurrentState, auto FunctionPointer, typename... Args>
struct return_of_final_state_t<FinalState<CurrentState, FunctionPointer>,
                               Args...> {
  using type = std::result_of_t<decltype(FunctionPointer)(Args...)>;
};

template <typename FinalStates, auto CurrentState, auto FunctionPointer,
          typename... Args>
using return_of_final_state =
    typename return_of_final_state_t<find_transition<CurrentState,
                                     FunctionPointer, FinalStates>, Args...>
        ::type;


// Get the result of a query, if it is valid (i.e. we have a valid query
// declared for this state and function pointer).
template <typename T, typename... Args>
struct return_of_valid_query_t{
  struct ValidQuery;
  struct Found;
  static_assert(std::is_same<ValidQuery, Found>::value,
                "Final state not found");
};

// Found a valid query. Needs the arguments for result_of.
template <auto CurrentState, auto FunctionPointer, typename... Args>
struct return_of_valid_query_t<ValidQuery<CurrentState, FunctionPointer>,
                               Args...> {
  using type = std::result_of_t<decltype(FunctionPointer)(Args...)>;
};

template <typename ValidQueries, auto CurrentState, auto FunctionPointer,
          typename... Args>
using return_of_valid_query =
    typename return_of_valid_query_t<
            find_transition<CurrentState, FunctionPointer, ValidQueries>,
            Args...>
        ::type;

template <auto State, typename InitialStates>
struct check_initial_state_v {
  static_assert(std::is_same_v<InitialStates, std::false_type>,
                "Wrong format for the initial states");
};

template <auto State, auto... InitialState>
struct check_initial_state_v<State, InitialStates<InitialState...>> {
  static constexpr bool value = ((State == InitialState) ||...);
};

// Check whether the pointer to member function is to an r-value qualified
// function. For example:
// class MyClass {
//   void l_value() const;
//   void r_value() &&;
// };
// is_pointer_to_r_value_member_function<&MyClass::l_value> -> false
// is_pointer_to_r_value_member_function<&MyClass::r_value> -> true
template<class T>
struct is_pointer_to_r_value_member_function : std::false_type {};

template<class R, class T, class... Args>
struct is_pointer_to_r_value_member_function<R (T::*)(Args...) &&>
  : std::true_type {};

// Generic wrapper: it's the class that checks the validity of the transitions,
// and calls the functions.
template<
         // The value identifying the state of the wrapper.
         auto CurrentState,
         // The wrapper type, inheriting the GenericWrapper. It is also
         // templated by the state.
         template <auto State> typename Wrapper,
         // The wrapped type (where we have the implementation of the functions
         // of the protocol).
         typename Wrapped,
         // The list of initial states. See PROTENC_START_WRAPPER.
         typename InitialStates,
         // The list of valid transitions. See PROTENC_START_WRAPPER.
         typename Transitions,
         // The list of valid end states. See PROTENC_START_WRAPPER.
         typename FinalStates,
         // The list of valid query function. See PROTENC_START_WRAPPER.
         typename ValidQueries
        >
class GenericWrapper {
 public:

  // Alias to this class, with a different state.
  template <auto NewState>
  using ThisWrapper = GenericWrapper<NewState, Wrapper, Wrapped, InitialStates,
                                     Transitions, FinalStates, ValidQueries>;

  // Check that the transition is valid, then call the function, and return the
  // wrapper with the updated state.
  template <auto FunctionPointer, typename... Args>
    auto call_transition(Args&&... args) && {
      (wrapped_.*FunctionPointer)(std::forward<Args>(args)...);
      constexpr auto target_state =
          return_of_transition<Transitions, CurrentState, FunctionPointer>;
      return Wrapper<target_state>(
                ThisWrapper<target_state>{std::move(wrapped_), true});
    }

  // Check that the final state is valid, then call the function and return the
  // result.
  template <auto FunctionPointer, typename... Args>
  auto call_final_state(Args&&... args) &&
    -> return_of_final_state<FinalStates, CurrentState, FunctionPointer,
                             Wrapped, Args...> {
    static_assert(is_pointer_to_r_value_member_function<
                      decltype(FunctionPointer)>::value,
                  "Final state functions should consume the object: "
                  "add && after the argument list.");
    return (std::move(wrapped_).*FunctionPointer)(std::forward<Args>(args)...);
  }

  // Check that the query is valid, then call the function and return the
  // result.
  template <auto FunctionPointer, typename... Args>
  auto call_valid_query(Args&&... args) const
    -> return_of_valid_query<ValidQueries, CurrentState, FunctionPointer,
                             Wrapped, Args...> {
    return (wrapped_.*FunctionPointer)(std::forward<Args>(args)...);
  }

 protected:
  // Needed to build the wrapper with a different state.
  template<auto, template <auto> typename, typename, typename, typename,
           typename, typename>
  friend class GenericWrapper;
  // Constructor. Only take by move to prevent accidental copy.
  GenericWrapper(Wrapped&& wrapped) : wrapped_(std::move(wrapped)) {
    this->template check_initial_state<CurrentState>();
  }
 private:
  GenericWrapper(Wrapped&& wrapped, bool ignore_check) : wrapped_(std::move(wrapped)) {}
  template <auto State>
  void check_initial_state() const {
    static_assert(check_initial_state_v<State, InitialStates>::value,
                  "State is not an initial state");
  }
  Wrapped wrapped_;
};

} // namespace internal
} // namespace prot_enc
