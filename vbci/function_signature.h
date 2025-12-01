#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>

// Extract function signature from callable types
template<typename T>
struct function_signature
{};

// Function pointers
template<typename R, typename... Args>
struct function_signature<R (*)(Args...)>
{
  using return_type = R;
  using args_tuple = std::tuple<Args...>;
  static constexpr size_t param_count = sizeof...(Args);
};

// Member function pointers
template<typename R, typename Class, typename... Args>
struct function_signature<R (Class::*)(Args...)>
{
  using return_type = R;
  using args_tuple = std::tuple<Args...>;
  static constexpr size_t param_count = sizeof...(Args);
};

// Const member function pointers (for lambdas)
template<typename R, typename Class, typename... Args>
struct function_signature<R (Class::*)(Args...) const>
{
  using return_type = R;
  using args_tuple = std::tuple<Args...>;
  static constexpr size_t param_count = sizeof...(Args);
};

// SFINAE helper to check if type has callable operator()
template<typename T, typename = void>
struct has_call_operator : std::false_type
{};

template<typename T>
struct has_call_operator<T, std::void_t<decltype(&T::operator())>>
: std::true_type
{};

// Unified function signature that handles both function pointers and lambdas
template<typename T>
struct unified_function_signature
{
  // For function pointers, use the function_signature directly
  // For lambdas/functors, extract the operator() signature
  using type = std::conditional_t<
    has_call_operator<T>::value,
    function_signature<decltype(&T::operator())>,
    function_signature<T>>;
};

// Specialization to avoid SFINAE issues with function pointers
template<typename R, typename... Args>
struct unified_function_signature<R (*)(Args...)>
{
  using type = function_signature<R (*)(Args...)>;
};

// Get the number of parameters for any callable
template<typename F>
static constexpr size_t param_count_v =
  unified_function_signature<F>::type::param_count;

// Get the ith parameter type (0-indexed)
template<typename F, size_t I>
using param_type_t = std::
  tuple_element_t<I, typename unified_function_signature<F>::type::args_tuple>;
