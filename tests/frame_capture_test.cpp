#include "FrameCapture.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void expectTrue(bool condition, const std::string &message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		++failures;
	}
}

void testEncoderWritesTopRowFirst()
{
	const std::vector<unsigned char> bottomUpPixels = {
		255, 0, 0, 0, 255, 0,
		0, 0, 255, 255, 255, 255,
	};
	const std::vector<unsigned char> encoded =
		encodeBottomUpRgbAsPpm(2, 2, bottomUpPixels);
	const std::string header = "P6\n2 2\n255\n";
	expectTrue(encoded.size() == header.size() + bottomUpPixels.size(),
	           "PPM output contains one header and every RGB byte");
	expectTrue(std::equal(header.begin(), header.end(), encoded.begin()),
	           "PPM output begins with the expected binary header");
	const std::vector<unsigned char> expectedTopDown = {
		0, 0, 255, 255, 255, 255,
		255, 0, 0, 0, 255, 0,
	};
	expectTrue(
		std::equal(expectedTopDown.begin(), expectedTopDown.end(),
		           encoded.begin() + static_cast<std::ptrdiff_t>(header.size())),
		"OpenGL bottom-up rows are reversed into top-down image order");
}

void testEncoderRejectsInvalidBuffers()
{
	expectTrue(encodeBottomUpRgbAsPpm(0, 2, {}).empty(),
	           "zero-width framebuffers are rejected");
	expectTrue(encodeBottomUpRgbAsPpm(2, 2, {1, 2, 3}).empty(),
	           "pixel counts that do not match dimensions are rejected");
}
}

int main()
{
	testEncoderWritesTopRowFirst();
	testEncoderRejectsInvalidBuffers();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Frame capture tests passed" << std::endl;
	return 0;
}
