#include "WorldLayout.h"
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

void testLandmarksCreateTwoReadableRegions()
{
	const auto &landmarks = worldLandmarkPlacements();
	int moonTrees = 0;
	int redSpires = 0;
	for (const WorldLandmarkPlacement &landmark : landmarks)
	{
		expectTrue(std::fabs(landmark.center.x) + landmark.collisionRadius < 46.0f &&
		               std::fabs(landmark.center.y) + landmark.collisionRadius < 46.0f,
		           "landmark collision stays inside the playable field");
		expectTrue(glm::length(landmark.center) > 12.0f,
		           "landmarks stay outside the camp teaching space");
		expectTrue(landmark.scale > 0.0f && landmark.collisionRadius > 0.0f &&
		               landmark.occlusionRadius >= landmark.collisionRadius &&
		               landmark.height >= 4.0f,
		           "landmarks have usable visual and collision dimensions");
		if (landmark.kind == WorldLandmarkKind::MoonTree)
		{
			++moonTrees;
			expectTrue(landmark.center.x < 0.0f && landmark.center.y < 0.0f,
			           "Moonshadow Edge occupies the forward-left field");
		}
		else
		{
			++redSpires;
			expectTrue(landmark.center.x > 0.0f && landmark.center.y < 0.0f,
			           "Redrock Highlands occupies the forward-right field");
		}
	}
	expectTrue(moonTrees >= 5, "Moonshadow Edge reads as a tree group");
	expectTrue(redSpires >= 3, "Redrock Highlands reads as a spire group");
}

void testLandmarkCollisionFootprintsDoNotOverlap()
{
	const auto &landmarks = worldLandmarkPlacements();
	for (std::size_t first = 0; first < landmarks.size(); ++first)
	{
		for (std::size_t second = first + 1; second < landmarks.size(); ++second)
		{
			const float separation = glm::distance(
				landmarks[first].center, landmarks[second].center);
			const float minimumSeparation = landmarks[first].collisionRadius +
			                                landmarks[second].collisionRadius + 0.25f;
			expectTrue(separation >= minimumSeparation,
			           "landmark collision footprints remain navigable");
		}
	}
}

void testTrailsCreateContinuousRoutesToBothRegions()
{
	const auto &segments = worldTrailSegments();
	int moonSegments = 0;
	int redSegments = 0;
	glm::vec2 moonEnd(0.0f);
	glm::vec2 redEnd(0.0f);
	for (const WorldTrailSegment &segment : segments)
	{
		expectTrue(glm::distance(segment.start, segment.end) >= 2.5f &&
		               segment.halfWidth >= 0.8f,
		           "trail segments remain broad enough to read and traverse");
		if (segment.kind == WorldTrailKind::MoonshadowRoute)
		{
			if (moonSegments == 0)
			{
				expectTrue(glm::distance(segment.start, glm::vec2(0.0f, -5.0f)) < 0.01f,
				           "Moonshadow route begins at the camp trailhead");
			}
			else
			{
				expectTrue(glm::distance(segment.start, moonEnd) < 0.01f,
				           "Moonshadow route segments stay connected");
			}
			moonEnd = segment.end;
			++moonSegments;
		}
		else
		{
			if (redSegments == 0)
			{
				expectTrue(glm::distance(segment.start, glm::vec2(0.0f, -5.0f)) < 0.01f,
				           "Redrock route begins at the camp trailhead");
			}
			else
			{
				expectTrue(glm::distance(segment.start, redEnd) < 0.01f,
				           "Redrock route segments stay connected");
			}
			redEnd = segment.end;
			++redSegments;
		}
	}
	expectTrue(moonSegments == 4 && moonEnd.x < -19.0f && moonEnd.y < -9.0f &&
	               dominantWorldRegion(moonEnd) == WorldRegionKind::MoonshadowEdge,
	           "Moonshadow route reaches the forest observation area");
	expectTrue(redSegments == 5 && redEnd.x > 26.0f && redEnd.y < -4.0f &&
	               dominantWorldRegion(redEnd) == WorldRegionKind::RedrockHighlands,
	           "Redrock route continues from the lookout to the Alpha nest");
}

void testTrailCoverageAndInterestPointsShareEndpoints()
{
	const auto &segments = worldTrailSegments();
	for (const WorldTrailSegment &segment : segments)
	{
		const glm::vec2 midpoint = (segment.start + segment.end) * 0.5f;
		const glm::vec2 direction = glm::normalize(segment.end - segment.start);
		const glm::vec2 perpendicular(-direction.y, direction.x);
		expectTrue(worldTrailCoverage(midpoint, segment) > 0.99f,
		           "trail center has full material coverage");
		expectTrue(worldTrailCoverage(
		               midpoint + perpendicular * (segment.halfWidth + 1.0f),
		               segment) < 0.01f,
		           "trail material fades away from the route");
	}

	const auto &points = worldInterestPointPlacements();
	expectTrue(points[0].kind == WorldInterestPointKind::Trailhead &&
	               glm::distance(points[0].center, segments[0].start) < 0.01f &&
	               glm::distance(points[0].center, segments[4].start) < 0.01f,
	           "trailhead marks the shared branch point");
	expectTrue(points[1].kind == WorldInterestPointKind::MoonshadowTracks &&
	               glm::distance(points[1].center, segments[3].end) < 0.01f &&
	               points[1].interactionRadius > points[1].visualRadius,
	           "Moonshadow interest point marks the forest route endpoint");
	expectTrue(points[2].kind == WorldInterestPointKind::RedrockLookout &&
	               glm::distance(points[2].center, segments[7].end) < 0.01f &&
	               points[2].interactionRadius > points[2].visualRadius,
	           "Redrock interest point marks the highland route endpoint");
	expectTrue(points[3].kind == WorldInterestPointKind::AlphaNest &&
	               dominantWorldRegion(points[3].center) ==
	                   WorldRegionKind::RedrockHighlands &&
	               glm::distance(points[3].center, segments[8].end) < 0.01f &&
	               glm::distance(points[3].center, points[2].center) > 6.0f &&
	               points[3].interactionRadius > points[3].visualRadius,
	           "the Alpha nest creates a separate terminal destination inside Redrock");
	expectTrue(points[0].interactionRadius == 0.0f,
	           "the shared trailhead remains guidance instead of a research trigger");
}
}

int main()
{
	testLandmarksCreateTwoReadableRegions();
	testLandmarkCollisionFootprintsDoNotOverlap();
	testTrailsCreateContinuousRoutesToBothRegions();
	testTrailCoverageAndInterestPointsShareEndpoints();
	if (failures != 0)
	{
		std::cerr << failures << " world layout test(s) failed" << std::endl;
		return 1;
	}
	std::cout << "All world layout tests passed" << std::endl;
	return 0;
}
