#include "WorldRegion.h"

#include <algorithm>

namespace
{
float regionInfluence(const glm::vec2 &position,
	                  const WorldRegionDescriptor &region)
{
	const float width = region.outerRadius - region.innerRadius;
	if (width <= 0.0f)
	{
		return 0.0f;
	}
	const float progress = glm::clamp(
		(glm::distance(position, region.center) - region.innerRadius) / width,
		0.0f, 1.0f);
	const float smoothProgress = progress * progress * (3.0f - 2.0f * progress);
	return 1.0f - smoothProgress;
}
}

const std::array<WorldRegionDescriptor, 3> &worldRegionDescriptors()
{
	static const std::array<WorldRegionDescriptor, 3> regions = {{
		{WorldRegionKind::WindwhisperMeadow, "Windwhisper Meadow",
		 {0.0f, 2.0f}, 0.0f, 0.0f, {0.34f, 0.55f, 0.24f}},
		{WorldRegionKind::MoonshadowEdge, "Moonshadow Edge",
		 {-22.0f, -21.0f}, 9.0f, 22.0f, {0.08f, 0.25f, 0.22f}},
		{WorldRegionKind::RedrockHighlands, "Redrock Highlands",
		 {20.0f, -12.0f}, 8.0f, 20.0f, {0.50f, 0.20f, 0.08f}},
	}};
	return regions;
}

const WorldRegionDescriptor &worldRegionDescriptor(WorldRegionKind kind)
{
	for (const WorldRegionDescriptor &region : worldRegionDescriptors())
	{
		if (region.kind == kind)
		{
			return region;
		}
	}
	return worldRegionDescriptors().front();
}

WorldRegionBlend sampleWorldRegionBlend(const glm::vec2 &worldPosition)
{
	WorldRegionBlend blend;
	blend.moonshadow = regionInfluence(
		worldPosition, worldRegionDescriptor(WorldRegionKind::MoonshadowEdge));
	blend.redrock = regionInfluence(
		worldPosition, worldRegionDescriptor(WorldRegionKind::RedrockHighlands));
	const float specialTotal = blend.moonshadow + blend.redrock;
	if (specialTotal > 1.0f)
	{
		blend.moonshadow /= specialTotal;
		blend.redrock /= specialTotal;
		blend.meadow = 0.0f;
	}
	else
	{
		blend.meadow = 1.0f - specialTotal;
	}
	return blend;
}

WorldRegionKind dominantWorldRegion(const glm::vec2 &worldPosition)
{
	const WorldRegionBlend blend = sampleWorldRegionBlend(worldPosition);
	if (blend.moonshadow > blend.meadow && blend.moonshadow >= blend.redrock)
	{
		return WorldRegionKind::MoonshadowEdge;
	}
	if (blend.redrock > blend.meadow && blend.redrock > blend.moonshadow)
	{
		return WorldRegionKind::RedrockHighlands;
	}
	return WorldRegionKind::WindwhisperMeadow;
}
