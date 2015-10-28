/*
The MIT License (MIT)

Copyright (c) 2015 Marcus Spangenberg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

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
