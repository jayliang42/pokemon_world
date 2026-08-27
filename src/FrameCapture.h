#pragma once

#include <string>
#include <vector>

std::vector<unsigned char> encodeBottomUpRgbAsPpm(
	int width, int height, const std::vector<unsigned char> &pixels);

bool writeBottomUpRgbPpm(
	const std::string &path, int width, int height,
	const std::vector<unsigned char> &pixels, std::string *errorMessage = nullptr);
