#pragma once

#include <vector>

class TerrainHeightMap
{
public:
	bool setPixels(int width, int height, const unsigned char *pixels, int channels);
	bool empty() const;

	float sampleNormalized(float u, float v) const;
	float heightAt(float worldX, float worldZ) const;

private:
	float sampleWrappedTexel(int x, int y) const;

	int width_ = 0;
	int height_ = 0;
	std::vector<float> redChannel_;
};
