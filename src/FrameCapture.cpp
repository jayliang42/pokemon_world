#include "FrameCapture.h"

#include <fstream>
#include <limits>
#include <sstream>

std::vector<unsigned char> encodeBottomUpRgbAsPpm(
	int width, int height, const std::vector<unsigned char> &pixels)
{
	if (width <= 0 || height <= 0)
	{
		return {};
	}
	const std::size_t pixelWidth = static_cast<std::size_t>(width);
	const std::size_t pixelHeight = static_cast<std::size_t>(height);
	if (pixelWidth > std::numeric_limits<std::size_t>::max() / 3u)
	{
		return {};
	}
	const std::size_t rowBytes = pixelWidth * 3u;
	if (pixelHeight > std::numeric_limits<std::size_t>::max() / rowBytes ||
	    pixels.size() != rowBytes * pixelHeight)
	{
		return {};
	}

	std::ostringstream header;
	header << "P6\n" << width << ' ' << height << "\n255\n";
	const std::string headerBytes = header.str();
	std::vector<unsigned char> encoded;
	encoded.reserve(headerBytes.size() + pixels.size());
	encoded.insert(encoded.end(), headerBytes.begin(), headerBytes.end());
	for (int row = height - 1; row >= 0; --row)
	{
		const std::size_t offset = static_cast<std::size_t>(row) * rowBytes;
		encoded.insert(encoded.end(), pixels.begin() + offset,
		               pixels.begin() + offset + rowBytes);
	}
	return encoded;
}

bool writeBottomUpRgbPpm(
	const std::string &path, int width, int height,
	const std::vector<unsigned char> &pixels, std::string *errorMessage)
{
	const std::vector<unsigned char> encoded =
		encodeBottomUpRgbAsPpm(width, height, pixels);
	if (encoded.empty())
	{
		if (errorMessage)
		{
			*errorMessage = "Invalid framebuffer dimensions or pixel count.";
		}
		return false;
	}
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output)
	{
		if (errorMessage)
		{
			*errorMessage = "Unable to open the capture output path.";
		}
		return false;
	}
	output.write(
		reinterpret_cast<const char *>(encoded.data()),
		static_cast<std::streamsize>(encoded.size()));
	if (!output)
	{
		if (errorMessage)
		{
			*errorMessage = "Unable to write the complete framebuffer capture.";
		}
		return false;
	}
	return true;
}
