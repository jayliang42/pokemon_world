#include "ResourceLocator.h"

#include <algorithm>

namespace
{
void appendUnique(std::vector<std::string> &candidates,
	              const std::string &candidate)
{
	if (!candidate.empty() &&
	    std::find(candidates.begin(), candidates.end(), candidate) ==
	        candidates.end())
	{
		candidates.push_back(candidate);
	}
}

std::string executableDirectory(const std::string &executablePath)
{
	const std::string::size_type separator = executablePath.find_last_of("/\\");
	return separator == std::string::npos
	           ? std::string()
	           : executablePath.substr(0, separator);
}
}

std::vector<std::string> resourceDirectoryCandidates(
	const std::string &executablePath,
	const std::string &requestedDirectory,
	bool explicitlyRequested)
{
	std::vector<std::string> candidates;
	appendUnique(candidates, requestedDirectory);
	if (explicitlyRequested)
	{
		return candidates;
	}

	appendUnique(candidates, "../resources");
	const std::string binaryDirectory = executableDirectory(executablePath);
	if (!binaryDirectory.empty())
	{
		appendUnique(candidates, binaryDirectory + "/../Resources");
		appendUnique(candidates, binaryDirectory + "/../resources");
		appendUnique(candidates, binaryDirectory + "/../../../../resources");
	}
	return candidates;
}

std::string locateResourceDirectory(
	const std::string &executablePath,
	const std::string &requestedDirectory,
	bool explicitlyRequested,
	const ResourceDirectoryProbe &hasResources)
{
	if (!hasResources)
	{
		return std::string();
	}
	for (const std::string &candidate : resourceDirectoryCandidates(
	         executablePath, requestedDirectory, explicitlyRequested))
	{
		if (hasResources(candidate))
		{
			return candidate;
		}
	}
	return std::string();
}
