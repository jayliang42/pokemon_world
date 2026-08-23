#include "TerrainHeightMap.h"

#include <cmath>
#include <cstddef>

namespace
{
constexpr float TERRAIN_SIZE = 100.0f;
constexpr float TERRAIN_HALF_SIZE = TERRAIN_SIZE * 0.5f;
constexpr float TERRAIN_HEIGHT_RANGE = 10.0f;
constexpr float TERRAIN_MIN_HEIGHT = -5.0f;

float wrapUnit(float value)
{
	return value - std::floor(value);
}

int wrapIndex(int value, int size)
{
	int wrapped = value % size;
	return wrapped < 0 ? wrapped + size : wrapped;
}

float mix(float first, float second, float amount)
{
	return first + (second - first) * amount;
}
}

bool TerrainHeightMap::setPixels(int width, int height, const unsigned char *pixels, int channels)
{
	width_ = 0;
	height_ = 0;
	redChannel_.clear();
	if (width <= 0 || height <= 0 || pixels == nullptr || channels <= 0)
	{
		return false;
	}

	const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
	redChannel_.resize(pixelCount);
	for (std::size_t index = 0; index < pixelCount; ++index)
	{
		redChannel_[index] = static_cast<float>(pixels[index * static_cast<std::size_t>(channels)]) / 255.0f;
	}
	width_ = width;
	height_ = height;
	return true;
}

bool TerrainHeightMap::empty() const
{
	return redChannel_.empty();
}

float TerrainHeightMap::sampleWrappedTexel(int x, int y) const
{
	const int wrappedX = wrapIndex(x, width_);
	const int wrappedY = wrapIndex(y, height_);
	const std::size_t index = static_cast<std::size_t>(wrappedY) * static_cast<std::size_t>(width_) +
	                          static_cast<std::size_t>(wrappedX);
	return redChannel_[index];
}

float TerrainHeightMap::sampleNormalized(float u, float v) const
{
	if (empty())
	{
		return 0.0f;
	}

	const float texelX = wrapUnit(u) * static_cast<float>(width_) - 0.5f;
	const float texelY = wrapUnit(v) * static_cast<float>(height_) - 0.5f;
	const int x0 = static_cast<int>(std::floor(texelX));
	const int y0 = static_cast<int>(std::floor(texelY));
	const float blendX = texelX - static_cast<float>(x0);
	const float blendY = texelY - static_cast<float>(y0);

	const float bottom = mix(sampleWrappedTexel(x0, y0), sampleWrappedTexel(x0 + 1, y0), blendX);
	const float top = mix(sampleWrappedTexel(x0, y0 + 1), sampleWrappedTexel(x0 + 1, y0 + 1), blendX);
	return mix(bottom, top, blendY);
}

float TerrainHeightMap::heightAt(float worldX, float worldZ) const
{
	if (empty())
	{
		return 0.0f;
	}

	const float gridX = std::floor(worldX);
	const float gridZ = std::floor(worldZ);
	const float blendX = worldX - gridX;
	const float blendZ = worldZ - gridZ;
	const float height00 = textureHeightAt(gridX, gridZ);
	const float height10 = textureHeightAt(gridX + 1.0f, gridZ);
	const float height01 = textureHeightAt(gridX, gridZ + 1.0f);
	const float height11 = textureHeightAt(gridX + 1.0f, gridZ + 1.0f);

	if (blendZ <= blendX)
	{
		return height00 * (1.0f - blendX) +
		       height10 * (blendX - blendZ) +
		       height11 * blendZ;
	}
	return height00 * (1.0f - blendZ) +
	       height11 * blendX +
	       height01 * (blendZ - blendX);
}

float TerrainHeightMap::textureHeightAt(float worldX, float worldZ) const
{
	const float u = (worldX + TERRAIN_HALF_SIZE) / TERRAIN_SIZE;
	const float v = (worldZ + TERRAIN_HALF_SIZE) / TERRAIN_SIZE;
	return sampleNormalized(u, v) * TERRAIN_HEIGHT_RANGE + TERRAIN_MIN_HEIGHT;
}
