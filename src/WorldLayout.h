#pragma once

#include <array>

#include <glm/glm.hpp>

struct WorldRockPlacement
{
	glm::vec2 center;
	glm::vec3 scale;
	float yaw = 0.0f;
};

enum class WorldLandmarkKind
{
	MoonTree,
	RedSpire,
};

struct WorldLandmarkPlacement
{
	WorldLandmarkKind kind = WorldLandmarkKind::MoonTree;
	glm::vec2 center = glm::vec2(0.0f);
	float scale = 1.0f;
	float yaw = 0.0f;
	float collisionRadius = 0.5f;
	float occlusionRadius = 1.0f;
	float height = 1.0f;
};

enum class WorldTrailKind
{
	MoonshadowRoute,
	RedrockRoute,
};

struct WorldTrailSegment
{
	WorldTrailKind kind = WorldTrailKind::MoonshadowRoute;
	glm::vec2 start = glm::vec2(0.0f);
	glm::vec2 end = glm::vec2(0.0f);
	float halfWidth = 1.0f;
};

enum class WorldInterestPointKind
{
	Trailhead,
	MoonshadowTracks,
	RedrockLookout,
	AlphaNest,
};

struct WorldInterestPointPlacement
{
	WorldInterestPointKind kind = WorldInterestPointKind::Trailhead;
	glm::vec2 center = glm::vec2(0.0f);
	float visualRadius = 1.0f;
	float interactionRadius = 1.0f;
};

const std::array<WorldRockPlacement, 10> &worldRockPlacements();
const std::array<WorldLandmarkPlacement, 9> &worldLandmarkPlacements();
const std::array<WorldTrailSegment, 9> &worldTrailSegments();
const std::array<WorldInterestPointPlacement, 4> &worldInterestPointPlacements();
float distanceToWorldTrailSegment(const glm::vec2 &position,
	                              const WorldTrailSegment &segment);
float worldTrailCoverage(const glm::vec2 &position,
	                     const WorldTrailSegment &segment,
	                     float featherWidth = 0.45f);
