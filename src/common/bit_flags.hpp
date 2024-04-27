#pragma once

#include <type_traits>

namespace nlrs
{
template<typename T>
concept Enum = std::is_enum_v<T>;

// A wrapper for scoped enums providing bitwise ops. Initialize like BitFlags<T>{T::Value1,
// T::Value2, ...}.
template<Enum T>
class BitFlags
{
public:
    constexpr BitFlags() noexcept = default;
    constexpr explicit BitFlags(T flag) noexcept
        : mFlags(static_cast<U>(flag))
    {
    }
    template<typename... Enums>
    constexpr BitFlags(Enums... flags) noexcept
    {
        (add(flags), ...);
    }

    static constexpr BitFlags none() noexcept { return BitFlags{static_cast<U>(0)}; }
    static constexpr BitFlags all() noexcept { return BitFlags{~static_cast<U>(0)}; }

    // Arithmetic operators

    constexpr bool has(const T flag) const noexcept
    {
        // NOTE: prefer to return a boolean rather than returning mFlags & static_cast<U>(flag), as
        // that value may not be represented in the scoped enum that is being wrapped.
        return (mFlags & static_cast<U>(flag)) == static_cast<U>(flag);
    }

    // Modifiers

    constexpr void add(const T flag) noexcept { mFlags |= static_cast<U>(flag); }

private:
    using U = std::underlying_type_t<T>;

    U mFlags = static_cast<U>(0);

    constexpr BitFlags(const U flags) noexcept
        : mFlags(flags)
    {
    }
};
} // namespace nlrs
