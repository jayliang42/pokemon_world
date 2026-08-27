#pragma once

#include <functional>
#include <string>
#include <vector>

using ResourceDirectoryProbe = std::function<bool(const std::string &)>;

std::vector<std::string> resourceDirectoryCandidates(
	const std::string &executablePath,
	const std::string &requestedDirectory,
	bool explicitlyRequested);

std::string locateResourceDirectory(
	const std::string &executablePath,
	const std::string &requestedDirectory,
	bool explicitlyRequested,
	const ResourceDirectoryProbe &hasResources);
