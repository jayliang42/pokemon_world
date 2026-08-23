#include "TerrainHeightMap.h"

#include <cmath>
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

void expectNear(float actual, float expected, float tolerance, const std::string &message)
{
	if (std::fabs(actual - expected) > tolerance)
	{
		std::cerr << "FAIL: " << message << " (expected " << expected
		          << ", got " << actual << ")" << std::endl;
		++failures;
	}
}

TerrainHeightMap makeTwoByTwoMap()
{
	const unsigned char pixels[] = {
		0, 0, 0, 255,
		64, 0, 0, 255,
		128, 0, 0, 255,
		255, 0, 0, 255,
	};

	TerrainHeightMap map;
	expectTrue(map.setPixels(2, 2, pixels, 4), "valid RGBA pixels are accepted");
	return map;
}

void testRejectsInvalidPixelData()
{
	TerrainHeightMap map;
	expectTrue(map.empty(), "new height map starts empty");
	expectTrue(!map.setPixels(0, 2, nullptr, 4), "invalid dimensions and null data are rejected");
	expectTrue(map.empty(), "rejected data leaves the height map empty");
}

void testMatchesOpenGLLinearRepeatSampling()
{
	TerrainHeightMap map = makeTwoByTwoMap();
	expectNear(map.sampleNormalized(0.25f, 0.25f), 0.0f, 0.00001f,
	           "first texel center samples the first red value");
	expectNear(map.sampleNormalized(0.75f, 0.25f), 64.0f / 255.0f, 0.00001f,
	           "second texel center samples the second red value");
	expectNear(map.sampleNormalized(0.25f, 0.75f), 128.0f / 255.0f, 0.00001f,
	           "third texel center preserves source row orientation");
	expectNear(map.sampleNormalized(0.75f, 0.75f), 1.0f, 0.00001f,
	           "fourth texel center samples the fourth red value");

	const float average = (0.0f + 64.0f + 128.0f + 255.0f) / (4.0f * 255.0f);
	expectNear(map.sampleNormalized(0.5f, 0.5f), average, 0.00001f,
	           "sample between four texels uses bilinear filtering");
	expectNear(map.sampleNormalized(1.25f, -0.75f), 0.0f, 0.00001f,
	           "coordinates outside the texture repeat on both axes");
}

void testMapsWorldCoordinatesToTerrainHeight()
{
	TerrainHeightMap map = makeTwoByTwoMap();
	expectNear(map.heightAt(-25.0f, -25.0f), -5.0f, 0.00001f,
	           "world-space lower quarter maps to the first height texel");
	expectNear(map.heightAt(25.0f, -25.0f), 64.0f / 255.0f * 10.0f - 5.0f, 0.00001f,
	           "world-space x maps across the full 100-unit terrain");
	expectNear(map.heightAt(-25.0f, 25.0f), 128.0f / 255.0f * 10.0f - 5.0f, 0.00001f,
	           "world-space z uses the same orientation as the GPU texture");
	expectNear(map.heightAt(75.0f, -25.0f), -5.0f, 0.00001f,
	           "terrain height repeats every 100 world units");
}

void testWorldHeightMatchesRenderedTriangleSurface()
{
	std::vector<unsigned char> pixels(200, 0);
	pixels[101] = 255;
	TerrainHeightMap map;
	expectTrue(map.setPixels(200, 1, pixels.data(), 1), "single-channel height pixels are accepted");

	expectNear(map.heightAt(0.0f, 0.0f), -5.0f, 0.00001f,
	           "rendered grid height starts at the first terrain vertex");
	expectNear(map.heightAt(1.0f, 0.0f), 0.0f, 0.00001f,
	           "rendered grid height reaches the next terrain vertex");
	expectNear(map.heightAt(0.5f, 0.0f), -2.5f, 0.00001f,
	           "collision height follows the rendered triangle between grid vertices");
}
}

int main()
{
	testRejectsInvalidPixelData();
	testMatchesOpenGLLinearRepeatSampling();
	testMapsWorldCoordinatesToTerrainHeight();
	testWorldHeightMatchesRenderedTriangleSurface();

	if (failures != 0)
	{
		std::cerr << failures << " terrain height map test(s) failed" << std::endl;
		return 1;
	}

	std::cout << "All terrain height map tests passed" << std::endl;
	return 0;
}
