#pragma once
#include <concepts>

template<class T>
concept only_move_constructible = std::move_constructible<T> && !std::copy_constructible<T>;

/**
 * \brief Copy-constructible container for move-constructible type T. Copying results in moving.
 */
template<only_move_constructible T>
struct copy_initiates_move final
{
	T object;
	explicit copy_initiates_move(T&& obj) : object(std::move(obj)) {}
	copy_initiates_move(const copy_initiates_move<T>& other)noexcept : object(std::move(const_cast<T&>(other.object)))
	{}
	copy_initiates_move& operator=(const copy_initiates_move<T>& other)
	{
		object = std::move(const_cast<T&>(other.object));
		return *this;
	}
};