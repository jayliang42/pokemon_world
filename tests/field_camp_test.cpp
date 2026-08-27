#include "FieldCamp.h"

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

void testDefaultLayoutContainsDistinctPhysicalStations()
{
	const FieldCampLayout camp = defaultFieldCampLayout();
	expectTrue(camp.interactionRadius > camp.landingRadius &&
	               camp.landingRadius > 0.0f,
	           "the camp interaction area surrounds a smaller landing zone");
	expectTrue(camp.wildExclusionRadius > camp.interactionRadius,
	           "wild Pokemon stay outside the full camp interaction area");
	expectTrue(glm::distance(camp.tentCenter, camp.workbenchCenter) > 2.0f &&
	               glm::distance(camp.tentCenter, camp.supplyCrateCenter) > 2.0f &&
	               glm::distance(camp.workbenchCenter,
	                             camp.supplyCrateCenter) > 1.0f,
	           "tent, workbench, and supply crate occupy distinct stations");
	expectTrue(glm::distance(camp.spawnPosition, camp.center) <=
	               camp.interactionRadius,
	           "the player starts inside the camp interaction area");
	expectTrue(glm::distance(camp.spawnPosition, camp.tentCenter) >
	               camp.landingRadius &&
	               glm::distance(camp.spawnPosition, camp.workbenchCenter) >
	                   camp.landingRadius &&
	               glm::distance(camp.spawnPosition, camp.supplyCrateCenter) >
	                   camp.landingRadius,
	           "the spawn keeps a clear composition gap from every camp station");
}

void testCampRangeUsesHorizontalWorldDistance()
{
	const FieldCampLayout camp = defaultFieldCampLayout();
	expectTrue(isInsideCampInteractionRange(glm::vec3(0.0f, 30.0f, 0.0f), camp),
	           "camp range is horizontal so altitude can be checked separately");
	expectTrue(!isInsideCampInteractionRange(
	               glm::vec3(camp.interactionRadius + 0.1f, 0.0f, 0.0f), camp),
	           "a player beyond the camp boundary cannot submit remotely");
	expectTrue(std::fabs(horizontalDistanceToCamp(
	               glm::vec3(3.0f, 9.0f, 4.0f), camp) - 5.0f) < 0.0001f,
	           "camp distance follows the XZ plane");
}
}

int main()
{
	testDefaultLayoutContainsDistinctPhysicalStations();
	testCampRangeUsesHorizontalWorldDistance();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Field camp tests passed" << std::endl;
	return 0;
}
