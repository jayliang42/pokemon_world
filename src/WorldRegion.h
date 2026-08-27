#pragma once

#include <array>

#include <glm/glm.hpp>

enum class WorldRegionKind
{
	WindwhisperMeadow,
	MoonshadowEdge,
	RedrockHighlands,
};

struct WorldRegionDescriptor
{
	WorldRegionKind kind = WorldRegionKind::WindwhisperMeadow;
	const char *name = "Windwhisper Meadow";
	glm::vec2 center = glm::vec2(0.0f);
	float innerRadius = 0.0f;
	float outerRadius = 0.0f;
	glm::vec3 terrainTint = glm::vec3(1.0f);
};

struct WorldRegionBlend
{
	float meadow = 1.0f;
	float moonshadow = 0.0f;
	float redrock = 0.0f;
};

const std::array<WorldRegionDescriptor, 3> &worldRegionDescriptors();
const WorldRegionDescriptor &worldRegionDescriptor(WorldRegionKind kind);
WorldRegionBlend sampleWorldRegionBlend(const glm::vec2 &worldPosition);
WorldRegionKind dominantWorldRegion(const glm::vec2 &worldPosition);
