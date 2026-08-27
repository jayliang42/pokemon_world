#include "PokemonSightline.h"

#include <iostream>
#include <limits>
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

void testFlatGroundLeavesRaisedSightlineClear()
{
	const bool clear = pokemonSightlineClear(
		glm::vec3(-5.0f, 1.4f, 0.0f), glm::vec3(5.0f, 1.0f, 0.0f), {},
		[](float, float) { return 0.0f; });
	expectTrue(clear, "flat ground does not hide a raised subject");
}

void testTerrainRidgeBlocksSightline()
{
	const bool clear = pokemonSightlineClear(
		glm::vec3(-5.0f, 1.2f, 0.0f), glm::vec3(5.0f, 1.2f, 0.0f), {},
		[](float x, float) { return x > -0.8f && x < 0.8f ? 3.0f : 0.0f; });
	expectTrue(!clear, "a terrain ridge between observer and subject blocks vision");
}

void testRockCylinderBlocksOnlyIntersectingRays()
{
	PokemonSightlineCylinder rock;
	rock.center = glm::vec2(0.0f);
	rock.radius = 1.0f;
	rock.baseY = 0.0f;
	rock.height = 3.0f;
	const auto flatGround = [](float, float) { return 0.0f; };
	expectTrue(!pokemonSightlineClear(
	               glm::vec3(-5.0f, 1.2f, 0.0f),
	               glm::vec3(5.0f, 1.2f, 0.0f), {rock}, flatGround),
	           "an intersecting rock cylinder blocks vision");
	expectTrue(pokemonSightlineClear(
	               glm::vec3(-5.0f, 1.2f, 3.0f),
	               glm::vec3(5.0f, 1.2f, 3.0f), {rock}, flatGround),
	           "a ray passing beside the rock stays clear");
	expectTrue(pokemonSightlineClear(
	               glm::vec3(-5.0f, 4.0f, 0.0f),
	               glm::vec3(5.0f, 4.0f, 0.0f), {rock}, flatGround),
	           "a ray above the rock stays clear");
}

void testInvalidGeometryFailsClosed()
{
	expectTrue(!pokemonSightlineClear(
	               glm::vec3(std::numeric_limits<float>::quiet_NaN()),
	               glm::vec3(1.0f), {}, [](float, float) { return 0.0f; }),
	           "non-finite sightline endpoints fail closed");
	expectTrue(!pokemonSightlineClear(
	               glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), {}, {}),
	           "a missing terrain provider fails closed");
}
}

int main()
{
	testFlatGroundLeavesRaisedSightlineClear();
	testTerrainRidgeBlocksSightline();
	testRockCylinderBlocksOnlyIntersectingRays();
	testInvalidGeometryFailsClosed();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Pokemon sightline tests passed" << std::endl;
	return 0;
}
