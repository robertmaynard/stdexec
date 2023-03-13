/*
 * Copyright (c) 2021-2022 NVIDIA Corporation
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#if __cpp_concepts < 201907L
#error This library requires support for C++20 concepts
#endif

#include <version>

// Perhaps the stdlib lacks support for concepts though:
#if __has_include(<concepts>) && __cpp_lib_concepts >= 202002
#define STDEXEC_HAS_STD_CONCEPTS_HEADER() 1
#else
#define STDEXEC_HAS_STD_CONCEPTS_HEADER() 0
#endif

#if STDEXEC_HAS_STD_CONCEPTS_HEADER()
#include <concepts>
#else
#include <type_traits>
#endif

namespace stdexec::__std_concepts {
#if defined(__clang__)
  template <class _A, class _B>
  concept __same_as = __is_same(_A, _B);
#elif defined(__GNUC__)
  template <class _A, class _B>
  concept __same_as = __is_same_as(_A, _B);
#else
  template <class _A, class _B>
  inline constexpr bool __same_as = false;
  template <class _A>
  inline constexpr bool __same_as<_A, _A> = true;
#endif

  // Make sure we're using a same_as concept that doesn't instantiate std::is_same
  template <class _A, class _B>
  concept same_as = __same_as<_A, _B> && __same_as<_B, _A>;

#if STDEXEC_HAS_STD_CONCEPTS_HEADER()

  using std::integral;
  using std::derived_from;
  using std::convertible_to;
  using std::equality_comparable;

#else

  template <class T>
  concept integral = std::is_integral_v<T>;

  template <class _A, class _B>
  concept derived_from =         //
    std::is_base_of_v<_B, _A> && //
    std::is_convertible_v<const volatile _A*, const volatile _B*>;

  template <class _From, class _To>
  concept convertible_to =               //
    std::is_convertible_v<_From, _To> && //
    requires(_From (&__fun)()) { static_cast<_To>(__fun()); };

  template <class _Ty>
  concept equality_comparable = //
    requires(const std::remove_reference_t<_Ty>& __t) {
      { __t == __t } -> convertible_to<bool>;
      { __t != __t } -> convertible_to<bool>;
    };
#endif
} // namespace stdexec::__std_concepts

namespace stdexec {
  using namespace __std_concepts;
  using std::decay_t;

  // // TODO: this makes nvc++ sad. Find out why.
  // template <class _Ty>
  //   _Ty __decay__(const _Ty&);
  // template <class _Ty>
  //   _Ty* __decay__(_Ty*);

  // template <class _Ty>
  //   auto __decay_(_Ty&&(*__fn)()) -> decltype((__decay__)(__fn()));
  // template <class>
  //   void __decay_(...);

  // template <class _Ty>
  //   using decay_t = decltype((__decay_<_Ty>)(0));

  // C++20 concepts
  template <class _Ty, class _U>
  concept __decays_to = __same_as<decay_t<_Ty>, _U>;

  template <class...>
  concept __true = true;

  template <class _C>
  concept __class = __true<int _C::*> && (!__same_as<const _C, _C>);

  template <class _Ty, class... _As>
  concept __one_of = (__same_as<_Ty, _As> || ...);

  template <class _Ty, class... _Us>
  concept __all_of = (__same_as<_Ty, _Us> && ...);

  template <class _Ty, class... _Us>
  concept __none_of = ((!__same_as<_Ty, _Us>) &&...);

  // Not exactly right, but close.
  template <class _Ty>
  concept __boolean_testable_ = convertible_to<_Ty, bool>;

  template <class _Ty>
  inline constexpr bool __is_lvalue_reference_ = false;
  template <class _Ty>
  inline constexpr bool __is_lvalue_reference_<_Ty&> = true;

  // Avoid using libstdc++'s object concepts because they instantiate a
  // lot of templates.
  template <class _Ty>
  inline constexpr bool __destructible_ = //
    requires {
      { ((_Ty && (*) () noexcept) nullptr)().~_Ty() } noexcept;
    };
  template <class _Ty>
  inline constexpr bool __destructible_<_Ty&> = true;
  template <class _Ty>
  inline constexpr bool __destructible_<_Ty&&> = true;
  template <class _Ty, std::size_t _N>
  inline constexpr bool __destructible_<_Ty[_N]> = __destructible_<_Ty>;

  template <class T>
  concept destructible = __destructible_<T>;

#if __has_builtin(__is_constructible)
  template <class _Ty, class... _As>
  concept constructible_from = //
    destructible<_Ty> &&       //
    __is_constructible(_Ty, _As...);
#else
  template <class _Ty, class... _As>
  concept constructible_from = //
    destructible<_Ty> &&       //
    is_constructible_v<_Ty, _As...>;
#endif

  template <class _Ty>
  concept default_initializable = //
    constructible_from<_Ty> &&    //
    requires { _Ty{}; } &&        //
    requires { ::new _Ty; };

  template <class _Ty>
  concept move_constructible = //
    constructible_from<_Ty, _Ty>;

  template <class _Ty>
  concept copy_constructible = //
    move_constructible<_Ty>    //
    && constructible_from<_Ty, _Ty const &>;

  template <class _LHS, class _RHS >
  concept assignable_from =         //
    __is_lvalue_reference_<_LHS> && //
    // std::common_reference_with<
    //   const std::remove_reference_t<_LHS>&,
    //   const std::remove_reference_t<_RHS>&> &&
    requires(_LHS __lhs, _RHS&& __rhs) {
      { __lhs = ((_RHS&&) __rhs) } -> same_as<_LHS>;
    };

  namespace __swap {
    using std::swap;

    template <class _Ty, class _Uy>
    concept swappable_with = //
      requires(_Ty&& __t, _Uy&& __u) { swap((_Ty&&) __t, (_Uy&&) __u); };

    inline constexpr auto const __fn = //
      []<class _Ty, swappable_with<_Ty> _Uy>(_Ty&& __t, _Uy&& __u) noexcept(
        noexcept(swap((_Ty&&) __t, (_Uy&&) __u))) {
        swap((_Ty&&) __t, (_Uy&&) __u);
      };
  }

  using __swap::swappable_with;
  inline constexpr auto const & swap = __swap::__fn;

  template <class _Ty>
  concept swappable = //
    swappable_with<_Ty, _Ty>;

  template < class _Ty >
  concept movable = std::is_object_v<_Ty> && move_constructible<_Ty> && assignable_from<_Ty&, _Ty>
                 && swappable<_Ty>;

  template <class _Ty>
  concept copyable =                     //
    copy_constructible<_Ty> &&           //
    movable<_Ty> &&                      //
    assignable_from<_Ty&, _Ty&> &&       //
    assignable_from<_Ty&, const _Ty&> && //
    assignable_from<_Ty&, const _Ty>;

  template <class _Ty>
  concept semiregular = //
    copyable<_Ty> &&    //
    default_initializable<_Ty>;

  template <class _Ty>
  concept regular =     //
    semiregular<_Ty> && //
    equality_comparable<_Ty>;

  template < class T, class U >
  concept __partially_ordered_with =       //
    requires(
      const std::remove_reference_t<T>& t, //
      const std::remove_reference_t<U>& u) {
      { t < u } -> __boolean_testable_;
      { t > u } -> __boolean_testable_;
      { t <= u } -> __boolean_testable_;
      { t >= u } -> __boolean_testable_;
      { u < t } -> __boolean_testable_;
      { u > t } -> __boolean_testable_;
      { u <= t } -> __boolean_testable_;
      { u >= t } -> __boolean_testable_;
    };

  template < class _Ty >
  concept totally_ordered =     //
    equality_comparable<_Ty> && //
    __partially_ordered_with<_Ty, _Ty>;

  template <class _Ty>
  concept __movable_value =             //
    move_constructible<decay_t<_Ty>> && //
    constructible_from<decay_t<_Ty>, _Ty>;

  template <class _Trait>
  concept __is_true = _Trait::value;

  template <class, template <class...> class>
  constexpr bool __is_instance_of_ = false;
  template <class... _As, template <class...> class _Ty>
  constexpr bool __is_instance_of_<_Ty<_As...>, _Ty> = true;

  template <class _Ay, template <class...> class _Ty>
  concept __is_instance_of = __is_instance_of_<_Ay, _Ty>;

  template <class _Ay, template <class...> class _Ty>
  concept __is_not_instance_of = !__is_instance_of<_Ay, _Ty>;

#if __has_builtin(__is_nothrow_constructible)
  template <class _Ty, class... _As>
  concept __nothrow_constructible_from =
    constructible_from<_Ty, _As...> && __is_nothrow_constructible(_Ty, _As...);
#else
  template <class _Ty, class... _As>
  concept __nothrow_constructible_from =
    constructible_from<_Ty, _As...> && std::is_nothrow_constructible_v<_Ty, _As...>;
#endif

  template <class _Ty>
  concept __decay_copyable = constructible_from<decay_t<_Ty>, _Ty>;

  template <class _Ty>
  concept __nothrow_decay_copyable = __nothrow_constructible_from<decay_t<_Ty>, _Ty>;
} // namespace stdexec

#if !STDEXEC_HAS_STD_CONCEPTS_HEADER()
namespace std {
  using namespace stdexec::__std_concepts;
}
#endif
