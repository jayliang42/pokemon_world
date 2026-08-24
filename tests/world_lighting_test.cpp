#include "WorldLighting.h"

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
		          << ", got " << actual << ')' << std::endl;
		++failures;
	}
}

bool isFinite(const glm::vec3 &value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) &&
	       std::isfinite(value.z);
}

void testCycleStartsAtNoonAndWraps()
{
	expectNear(worldLightingCyclePhase(0.0), 0.25f, 0.0001f,
	           "the first frame starts at noon");
	expectNear(worldLightingCyclePhase(WORLD_LIGHTING_CYCLE_SECONDS), 0.25f,
	           0.0001f, "one full lighting cycle wraps to the starting phase");
}

void testNoonIsBrightAndAboveTheField()
{
	const WorldLighting noon = sampleWorldLighting(0.25f);
	expectTrue(noon.daylight > 0.99f, "noon has full daylight");
	expectTrue(noon.sunDirection.y > 0.70f,
	           "noon sun remains above the field");
	expectTrue(noon.ambientColor.x > 0.45f,
	           "noon retains a readable ambient light level");
	expectTrue(noon.fogEnd > noon.fogStart,
	           "noon fog range is ordered");
}

void testMidnightIsDimAndBelowTheField()
{
	const WorldLighting noon = sampleWorldLighting(0.25f);
	const WorldLighting midnight = sampleWorldLighting(0.75f);
	expectTrue(midnight.daylight < 0.01f, "midnight has no daylight");
	expectTrue(midnight.sunDirection.y < -0.70f,
	           "midnight sun is below the field");
	expectTrue(midnight.ambientColor.x < noon.ambientColor.x,
	           "midnight ambient light is dimmer than noon");
	expectTrue(midnight.fogColor.x < noon.fogColor.x,
	           "midnight fog shifts darker than noon");
	expectTrue(midnight.fogStart < noon.fogStart,
	           "midnight fog begins closer to the player");
}

void testSamplesStayFiniteAndNormalized()
{
	const float phases[] = {0.0f, 0.125f, 0.25f, 0.5f, 0.75f, 0.999f};
	for (float phase : phases)
	{
		const WorldLighting lighting = sampleWorldLighting(phase);
		expectNear(glm::length(lighting.sunDirection), 1.0f, 0.0001f,
		           "sun direction remains normalized");
		expectTrue(isFinite(lighting.sunColor) &&
		               isFinite(lighting.ambientColor) &&
		               isFinite(lighting.fogColor),
		           "lighting colors remain finite");
		expectTrue(glm::all(glm::greaterThanEqual(lighting.sunColor, glm::vec3(0.0f))) &&
		               glm::all(glm::greaterThanEqual(lighting.ambientColor, glm::vec3(0.0f))) &&
		               glm::all(glm::greaterThanEqual(lighting.fogColor, glm::vec3(0.0f))),
		           "lighting colors remain non-negative");
	}
}
}

int main()
{
	testCycleStartsAtNoonAndWraps();
	testNoonIsBrightAndAboveTheField();
	testMidnightIsDimAndBelowTheField();
	testSamplesStayFiniteAndNormalized();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "World lighting tests passed" << std::endl;
	return 0;
}
