#pragma once

#include <cmath>
#include <cstdint>

namespace AudioChip {


class SineTable {
public:
	SineTable() {
		const float phaseIncrementPerStep = pi2 / static_cast<float>(size);
		for (uint32_t i = 0; i < size; ++i) {
			float phase = phaseIncrementPerStep * static_cast<float>(i);
			data[i] = sinf(phase);
		}
	}

	inline float lookupSinf(const float inPhase) {
		const uint32_t step = static_cast<uint32_t>((inPhase / pi2) * static_cast<float>(size)) & mask;
		const uint32_t step2 = step + 1;
		return (data[step] + data[step2]) / 2.0f;
	}

private:
	static constexpr float pi2 = 2.0f * M_PI;
	static const uint32_t size = 4096;
	static const uint32_t mask = size - 1;
	float data[size];
};


} // namespace AudioChip
