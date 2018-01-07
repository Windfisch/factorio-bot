#include <limits>
#include <stdexcept>
#include <type_traits>
#include <cassert>

template <typename To, typename From> // signed -> signed
static typename std::enable_if<std::is_signed<To>::value && std::is_signed<From>::value, To>::type safe_cast(From value)
{
    assert (value >= std::numeric_limits<To>::min() && value <= std::numeric_limits<To>::max());
    return static_cast<To>(value);
}

template <typename To, typename From> // signed -> unsigned
static typename std::enable_if<std::is_signed<To>::value && std::is_unsigned<From>::value, To>::type safe_cast(From value)
{
    assert (value <= static_cast<typename std::make_unsigned<To>::type>(std::numeric_limits<To>::max()));
    return static_cast<To>(value);
}

template <typename To, typename From> // unsigned -> signed
static typename std::enable_if<std::is_unsigned<To>::value && std::is_signed<From>::value, To>::type safe_cast(From value)
{
    assert (value >= 0 && static_cast<typename std::make_unsigned<From>::type>(value) <= std::numeric_limits<To>::max());
    return static_cast<To>(value);
}

template <typename To, typename From> // unsigned -> unsigned
static typename std::enable_if<std::is_unsigned<To>::value && std::is_unsigned<From>::value, To>::type safe_cast(From value)
{
    assert (value <= std::numeric_limits<To>::max());
    return static_cast<To>(value);
}
