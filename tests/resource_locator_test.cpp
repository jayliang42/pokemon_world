#include "ResourceLocator.h"

#include <iostream>
#include <string>
#include <vector>

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

void testExplicitDirectoryDoesNotFallBack()
{
	const std::vector<std::string> candidates = resourceDirectoryCandidates(
		"/repo/build/final", "/custom/resources", true);
	expectTrue(candidates.size() == 1 && candidates[0] == "/custom/resources",
	           "an explicit resource directory remains authoritative");
	expectTrue(locateResourceDirectory(
	               "/repo/build/final", "/missing", true,
	               [](const std::string &path) { return path == "../resources"; })
	               .empty(),
	           "an invalid explicit directory does not silently fall back");
}

void testCommandLineBuildFindsProjectResources()
{
	const std::string located = locateResourceDirectory(
		"/repo/build/final", "resources", false,
		[](const std::string &path) {
			return path == "/repo/build/../resources";
		});
	expectTrue(located == "/repo/build/../resources",
	           "a build-directory executable finds the project resources");
}

void testMacBundleFindsPackagedResources()
{
	const std::string bundleResources =
		"/repo/build/Pokemon World.app/Contents/MacOS/../Resources";
	const std::string located = locateResourceDirectory(
		"/repo/build/Pokemon World.app/Contents/MacOS/Pokemon World",
		"resources", false,
		[&bundleResources](const std::string &path) {
			return path == bundleResources;
		});
	expectTrue(located == bundleResources,
	           "a macOS app resolves its Contents/Resources directory");
}
}

int main()
{
	testExplicitDirectoryDoesNotFallBack();
	testCommandLineBuildFindsProjectResources();
	testMacBundleFindsPackagedResources();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Resource locator tests passed" << std::endl;
	return 0;
}
