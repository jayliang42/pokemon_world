#include "WorldLayout.h"

#include <algorithm>
#include <cmath>

const std::array<WorldRockPlacement, 10> &worldRockPlacements()
{
	static const std::array<WorldRockPlacement, 10> placements = {{
		{{5.0f, -7.0f}, {1.0f, 0.70f, 0.85f}, 0.25f},
		{{-7.0f, -10.0f}, {0.8f, 0.90f, 0.75f}, -0.4f},
		{{12.0f, -4.0f}, {1.25f, 0.75f, 0.95f}, 0.75f},
		{{-13.0f, 4.0f}, {1.05f, 0.60f, 0.90f}, 0.1f},
		{{8.0f, 12.0f}, {0.9f, 1.05f, 0.80f}, -0.65f},
		{{-5.0f, 15.0f}, {1.15f, 0.70f, 1.0f}, 0.45f},
		{{18.0f, -16.0f}, {1.4f, 0.85f, 1.05f}, -0.2f},
		{{-19.0f, -14.0f}, {1.0f, 1.15f, 0.90f}, 0.6f},
		{{22.0f, 9.0f}, {1.15f, 0.65f, 1.30f}, -0.8f},
		{{-23.0f, 18.0f}, {1.3f, 0.75f, 1.0f}, 0.3f},
	}};
	return placements;
}

const std::array<WorldLandmarkPlacement, 9> &worldLandmarkPlacements()
{
	static const std::array<WorldLandmarkPlacement, 9> placements = {{
		{WorldLandmarkKind::MoonTree, {-15.0f, -18.0f}, 0.90f, 0.10f,
		 0.37f, 1.58f, 4.28f},
		{WorldLandmarkKind::MoonTree, {-20.0f, -20.0f}, 1.15f, -0.24f,
		 0.47f, 2.01f, 5.46f},
		{WorldLandmarkKind::MoonTree, {-25.0f, -18.0f}, 0.95f, 0.42f,
		 0.39f, 1.66f, 4.51f},
		{WorldLandmarkKind::MoonTree, {-18.0f, -25.0f}, 1.10f, -0.52f,
		 0.45f, 1.93f, 5.23f},
		{WorldLandmarkKind::MoonTree, {-24.0f, -27.0f}, 1.25f, 0.28f,
		 0.51f, 2.19f, 5.94f},
		{WorldLandmarkKind::MoonTree, {-30.0f, -23.0f}, 1.05f, -0.08f,
		 0.43f, 1.84f, 4.99f},
		{WorldLandmarkKind::RedSpire, {13.0f, -12.0f}, 0.78f, -0.18f,
		 1.13f, 1.13f, 5.58f},
		{WorldLandmarkKind::RedSpire, {20.0f, -10.0f}, 1.18f, 0.20f,
		 1.71f, 1.71f, 8.44f},
		{WorldLandmarkKind::RedSpire, {27.0f, -13.0f}, 0.72f, -0.42f,
		 1.04f, 1.04f, 5.15f},
	}};
	return placements;
}

const std::array<WorldTrailSegment, 9> &worldTrailSegments()
{
	static const std::array<WorldTrailSegment, 9> segments = {{
		{WorldTrailKind::MoonshadowRoute, {0.0f, -5.0f}, {-6.5f, -8.5f}, 1.10f},
		{WorldTrailKind::MoonshadowRoute, {-6.5f, -8.5f}, {-12.0f, -12.0f}, 1.05f},
		{WorldTrailKind::MoonshadowRoute, {-12.0f, -12.0f}, {-16.0f, -12.5f}, 0.95f},
		{WorldTrailKind::MoonshadowRoute, {-16.0f, -12.5f}, {-19.5f, -10.0f}, 0.90f},
		{WorldTrailKind::RedrockRoute, {0.0f, -5.0f}, {6.5f, -7.0f}, 1.10f},
		{WorldTrailKind::RedrockRoute, {6.5f, -7.0f}, {12.5f, -8.5f}, 1.05f},
		{WorldTrailKind::RedrockRoute, {12.5f, -8.5f}, {16.0f, -6.0f}, 0.95f},
		{WorldTrailKind::RedrockRoute, {16.0f, -6.0f}, {18.0f, -3.5f}, 0.90f},
		{WorldTrailKind::RedrockRoute, {18.0f, -3.5f}, {31.5f, -5.5f}, 0.88f},
	}};
	return segments;
}

const std::array<WorldInterestPointPlacement, 4> &worldInterestPointPlacements()
{
	static const std::array<WorldInterestPointPlacement, 4> placements = {{
		{WorldInterestPointKind::Trailhead, {0.0f, -5.0f}, 1.55f, 0.0f},
		{WorldInterestPointKind::MoonshadowTracks, {-19.5f, -10.0f}, 1.85f, 2.4f},
		{WorldInterestPointKind::RedrockLookout, {18.0f, -3.5f}, 1.85f, 2.4f},
		{WorldInterestPointKind::AlphaNest, {31.5f, -5.5f}, 2.45f, 3.2f},
	}};
	return placements;
}

float distanceToWorldTrailSegment(const glm::vec2 &position,
	                              const WorldTrailSegment &segment)
{
	const glm::vec2 delta = segment.end - segment.start;
	const float lengthSquared = glm::dot(delta, delta);
	if (lengthSquared <= 0.000001f)
	{
		return glm::distance(position, segment.start);
	}
	const float progress = std::max(
		0.0f, std::min(1.0f,
		               glm::dot(position - segment.start, delta) / lengthSquared));
	return glm::distance(position, segment.start + delta * progress);
}

float worldTrailCoverage(const glm::vec2 &position,
	                     const WorldTrailSegment &segment,
	                     float featherWidth)
{
	const float feather = std::max(0.001f, featherWidth);
	const float distance = distanceToWorldTrailSegment(position, segment);
	const float edgeProgress = std::max(
		0.0f, std::min(1.0f,
		               (distance - segment.halfWidth) / feather));
	const float smoothEdge = edgeProgress * edgeProgress * (3.0f - 2.0f * edgeProgress);
	return 1.0f - smoothEdge;
}
