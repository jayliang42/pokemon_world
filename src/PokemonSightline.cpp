#include "PokemonSightline.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float EPSILON = 0.000001f;
constexpr float TERRAIN_CLEARANCE = 0.12f;
constexpr float TERRAIN_SAMPLE_SPACING = 0.5f;
constexpr int MAXIMUM_TERRAIN_SAMPLES = 256;

bool finiteVector(const glm::vec3 &value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) &&
	       std::isfinite(value.z);
}

bool intersectIntervals(float firstMinimum, float firstMaximum,
	                    float secondMinimum, float secondMaximum,
	                    float *entry, float *exit)
{
	*entry = std::max(firstMinimum, secondMinimum);
	*exit = std::min(firstMaximum, secondMaximum);
	return *entry <= *exit;
}

bool cylinderBlocks(const glm::vec3 &start, const glm::vec3 &end,
	                const PokemonSightlineCylinder &cylinder)
{
	if (!std::isfinite(cylinder.center.x) ||
	    !std::isfinite(cylinder.center.y) ||
	    !std::isfinite(cylinder.radius) ||
	    !std::isfinite(cylinder.baseY) ||
	    !std::isfinite(cylinder.height) || cylinder.radius <= 0.0f ||
	    cylinder.height <= 0.0f)
	{
		return true;
	}

	const glm::vec3 segment = end - start;
	const glm::vec2 horizontalStart(start.x - cylinder.center.x,
	                                start.z - cylinder.center.y);
	const glm::vec2 horizontalDelta(segment.x, segment.z);
	const float horizontalLengthSquared =
		glm::dot(horizontalDelta, horizontalDelta);
	float horizontalEntry = 0.0f;
	float horizontalExit = 1.0f;
	if (horizontalLengthSquared <= EPSILON)
	{
		if (glm::dot(horizontalStart, horizontalStart) >
		    cylinder.radius * cylinder.radius)
		{
			return false;
		}
	}
	else
	{
		const float b = glm::dot(horizontalStart, horizontalDelta);
		const float c = glm::dot(horizontalStart, horizontalStart) -
		                cylinder.radius * cylinder.radius;
		const float discriminant =
			b * b - horizontalLengthSquared * c;
		if (discriminant < 0.0f)
		{
			return false;
		}
		const float root = std::sqrt(discriminant);
		horizontalEntry = (-b - root) / horizontalLengthSquared;
		horizontalExit = (-b + root) / horizontalLengthSquared;
	}

	float verticalEntry = 0.0f;
	float verticalExit = 1.0f;
	const float maximumY = cylinder.baseY + cylinder.height;
	if (std::fabs(segment.y) <= EPSILON)
	{
		if (start.y < cylinder.baseY || start.y > maximumY)
		{
			return false;
		}
	}
	else
	{
		const float first = (cylinder.baseY - start.y) / segment.y;
		const float second = (maximumY - start.y) / segment.y;
		verticalEntry = std::min(first, second);
		verticalExit = std::max(first, second);
	}

	float entry = 0.0f;
	float exit = 1.0f;
	return intersectIntervals(horizontalEntry, horizontalExit, verticalEntry,
	                          verticalExit, &entry, &exit) &&
	       intersectIntervals(entry, exit, 0.0f, 1.0f, &entry, &exit);
}
}

bool pokemonSightlineClear(
	const glm::vec3 &observerEye, const glm::vec3 &subjectCenter,
	const std::vector<PokemonSightlineCylinder> &cylinders,
	const PokemonSightlineHeightProvider &groundHeightProvider)
{
	if (!finiteVector(observerEye) || !finiteVector(subjectCenter) ||
	    !groundHeightProvider)
	{
		return false;
	}
	for (const PokemonSightlineCylinder &cylinder : cylinders)
	{
		if (cylinderBlocks(observerEye, subjectCenter, cylinder))
		{
			return false;
		}
	}

	const glm::vec3 segment = subjectCenter - observerEye;
	const float horizontalDistance =
		glm::length(glm::vec2(segment.x, segment.z));
	const int samples = std::max(
		1, std::min(MAXIMUM_TERRAIN_SAMPLES,
		            static_cast<int>(std::ceil(
		                horizontalDistance / TERRAIN_SAMPLE_SPACING))));
	for (int sample = 1; sample < samples; ++sample)
	{
		const float fraction =
			static_cast<float>(sample) / static_cast<float>(samples);
		const glm::vec3 position = observerEye + segment * fraction;
		const float terrainHeight =
			groundHeightProvider(position.x, position.z);
		if (!std::isfinite(terrainHeight) ||
		    terrainHeight + TERRAIN_CLEARANCE >= position.y)
		{
			return false;
		}
	}
	return true;
}
