#pragma once

/*
This header implements bs_t<> class for scoped enum types (enum class).
To enable bs_t<>, enum scope must contain `bitset_last` entry.

enum class flagzz : u32
{
    flag1, // Bit indices start from zero
    flag2,

    bitset_last // It must be the last value
};

This also enables helper operators for this enum type.

Examples:
`+flagzz::flag1` - unary `+` operator convert flagzz value to bs_t<flagzz>
`flagzz::flag1 + flagzz::flag2` - bitset union
`flagzz::flag1 - flagzz::flag2` - bitset difference
Intersection (&) and symmetric difference (^) is also available.
*/

#include "rx/EnumBitSet.hpp"
#include "util/atomic.hpp"
#include "util/StrFmt.h"

template <typename T>
concept BitSetEnum = std::is_enum_v<T> && requires(T x) {
	T::bitset_last;
};

// Atomic bitset specialization with optimized operations
template <BitSetEnum T>
class atomic_bs_t : public atomic_t<rx::EnumBitSet<T>>
{
	// Corresponding bitset type
	using bs_t = rx::EnumBitSet<T>;

	// Base class
	using base = atomic_t<rx::EnumBitSet<T>>;

	// Use underlying m_data
	using base::m_data;

public:
	// Underlying type
	using underlying_type = typename bs_t::underlying_type;

	atomic_bs_t() = default;

	atomic_bs_t(const atomic_bs_t&) = delete;

	atomic_bs_t& operator=(const atomic_bs_t&) = delete;

	explicit constexpr atomic_bs_t(bs_t value)
		: base(value)
	{
	}

	explicit constexpr atomic_bs_t(T bit)
		: base(bit)
	{
	}

	using base::operator bs_t;

	explicit operator bool() const
	{
		return static_cast<bool>(base::load());
	}

	explicit operator underlying_type() const
	{
		return static_cast<underlying_type>(base::load());
	}

	bs_t operator+() const
	{
		return base::load();
	}

	bs_t fetch_add(const bs_t& rhs)
	{
		return bs_t::fromUnderlying(atomic_storage<underlying_type>::fetch_or(m_data.raw(), rhs.toUnderlying()));
	}

	bs_t add_fetch(const bs_t& rhs)
	{
		return bs_t::fromUnderlying(atomic_storage<underlying_type>::or_fetch(m_data.raw(), rhs.toUnderlying()));
	}

	bs_t operator+=(const bs_t& rhs)
	{
		return add_fetch(rhs);
	}

	bs_t fetch_sub(const bs_t& rhs)
	{
		return bs_t::fromUnderlying(atomic_storage<underlying_type>::fetch_and(m_data.raw(), ~rhs.toUnderlying()));
	}

	bs_t sub_fetch(const bs_t& rhs)
	{
		return bs_t::fromUnderlying(atomic_storage<underlying_type>::and_fetch(m_data.raw(), ~rhs.toUnderlying()));
	}

	bs_t operator-=(const bs_t& rhs)
	{
		return sub_fetch(rhs);
	}

	bs_t fetch_and(const bs_t& rhs)
	{
		return bs_t::fromUnderlying(atomic_storage<underlying_type>::fetch_and(m_data.raw(), rhs.toUnderlying()));
	}

	bs_t and_fetch(const bs_t& rhs)
	{
		return bs_t::fromUnderlying(atomic_storage<underlying_type>::and_fetch(m_data.raw(), rhs.toUnderlying()));
	}

	bs_t operator&=(const bs_t& rhs)
	{
		return and_fetch(rhs);
	}

	bs_t fetch_xor(const bs_t& rhs)
	{
		return bs_t::fromUnderlying(atomic_storage<underlying_type>::fetch_xor(m_data.raw(), rhs.toUnderlying()));
	}

	bs_t xor_fetch(const bs_t& rhs)
	{
		return bs_t::fromUnderlying(atomic_storage<underlying_type>::xor_fetch(m_data.raw(), rhs.toUnderlying()));
	}

	bs_t operator^=(const bs_t& rhs)
	{
		return xor_fetch(rhs);
	}

	auto fetch_or(const bs_t&) = delete;
	auto or_fetch(const bs_t&) = delete;
	auto operator|=(const bs_t&) = delete;

	bool test_and_set(T rhs)
	{
		return atomic_storage<underlying_type>::bts(m_data.raw(), static_cast<uint>(static_cast<underlying_type>(rhs)));
	}

	bool test_and_reset(T rhs)
	{
		return atomic_storage<underlying_type>::btr(m_data.raw(), static_cast<uint>(static_cast<underlying_type>(rhs)));
	}

	bool test_and_invert(T rhs)
	{
		return atomic_storage<underlying_type>::btc(m_data.raw(), static_cast<uint>(static_cast<underlying_type>(rhs)));
	}

	bool bit_test_set(uint bit) = delete;
	bool bit_test_reset(uint bit) = delete;
	bool bit_test_invert(uint bit) = delete;

	bool all_of(bs_t arg) const
	{
		return base::load().all_of(arg);
	}

	bool none_of(bs_t arg) const
	{
		return base::load().none_of(arg);
	}
};
