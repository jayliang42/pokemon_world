#include "WorldRegion.h"

#include <cmath>
#include <iostream>
#include <string>

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

void expectNear(float actual, float expected, float tolerance,
	            const std::string &message)
{
	if (std::fabs(actual - expected) > tolerance)
	{
		std::cerr << "FAIL: " << message << " (expected " << expected
		          << ", got " << actual << ")" << std::endl;
		++failures;
	}
}

void testThreeRegionCentersAreReadable()
{
	const auto &regions = worldRegionDescriptors();
	expectTrue(regions.size() == 3, "the field exposes exactly three regions");
	expectTrue(dominantWorldRegion(glm::vec2(0.0f)) ==
	               WorldRegionKind::WindwhisperMeadow,
	           "the physical camp remains inside Windwhisper Meadow");
	expectTrue(dominantWorldRegion(glm::vec2(-22.0f, -21.0f)) ==
	               WorldRegionKind::MoonshadowEdge,
	           "the moon-tree cluster anchors Moonshadow Edge");
	expectTrue(dominantWorldRegion(glm::vec2(20.0f, -12.0f)) ==
	               WorldRegionKind::RedrockHighlands,
	           "the crystal spires anchor Redrock Highlands");
}

void testRegionBlendingIsContinuousAndNormalized()
{
	for (int x = -46; x <= 46; ++x)
	{
		for (int z = -46; z <= 46; ++z)
		{
			const WorldRegionBlend blend = sampleWorldRegionBlend(
				glm::vec2(static_cast<float>(x), static_cast<float>(z)));
			expectNear(blend.meadow + blend.moonshadow + blend.redrock,
			           1.0f, 0.0001f, "region blend weights remain normalized");
			expectTrue(blend.meadow >= 0.0f && blend.moonshadow >= 0.0f &&
			               blend.redrock >= 0.0f,
			           "region blend weights never become negative");
		}
	}
	const WorldRegionBlend moonCenter = sampleWorldRegionBlend({-22.0f, -21.0f});
	const WorldRegionBlend moonEdge = sampleWorldRegionBlend({-5.0f, -21.0f});
	expectTrue(moonCenter.moonshadow > moonEdge.moonshadow &&
	               moonEdge.moonshadow > 0.0f,
	           "Moonshadow color fades through a visible transition band");
}
}

int main()
{
	testThreeRegionCentersAreReadable();
	testRegionBlendingIsContinuousAndNormalized();
	if (failures != 0)
	{
		std::cerr << failures << " world region test(s) failed" << std::endl;
		return 1;
	}
	std::cout << "All world region tests passed" << std::endl;
	return 0;
}
