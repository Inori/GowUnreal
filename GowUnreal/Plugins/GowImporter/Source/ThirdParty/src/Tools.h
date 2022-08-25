#pragma once

#include <cstdint>

namespace bit
{
	template <typename T>
	T extract(T value, uint32_t lst, uint32_t fst)
	{
		return (value >> fst) & ~(~T(0) << (lst - fst + 1));
	}
}  // namespace bit
