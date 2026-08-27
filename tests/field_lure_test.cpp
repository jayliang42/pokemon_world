#include "FieldLure.h"

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

void testDeploymentRequiresUnlockInventoryAndGrounding()
{
	const glm::vec3 position(4.0f, 1.0f, -2.0f);
	expectTrue(deployFieldLure(false, 2, true, true, position).status ==
	               FieldLureDeployStatus::Locked,
	           "Trainees cannot deploy the locked research tool");
	expectTrue(deployFieldLure(true, 0, true, true, position).status ==
	               FieldLureDeployStatus::Empty,
	           "deployment requires remaining lure inventory");
	expectTrue(deployFieldLure(true, 2, false, true, position).status ==
	               FieldLureDeployStatus::Airborne,
	           "a lure must be deliberately placed from the ground");
	expectTrue(deployFieldLure(true, 2, true, false, position).status ==
	               FieldLureDeployStatus::Unavailable,
	           "ended or camp-locked gameplay cannot deploy a lure");

	const FieldLureDeployResult deployed =
		deployFieldLure(true, 2, true, true, position);
	expectTrue(deployed.status == FieldLureDeployStatus::Deployed &&
	               deployed.remainingInventory == 1 && deployed.lure.active &&
	               deployed.lure.position == position &&
	               std::fabs(deployed.lure.remainingSeconds - FIELD_LURE_DURATION_SECONDS) <
	                   0.0001f,
	           "valid deployment consumes one lure and starts its field timer");
	expectTrue(deployFieldLure(true, 1, true, true, position,
	                          deployed.lure.active)
	                   .status == FieldLureDeployStatus::AlreadyActive,
	           "an active lure cannot be overwritten to waste inventory");
}

void testOnlyCalmNearbyEeveeFollowTheLure()
{
	FieldLureState lure;
	lure.active = true;
	lure.position = glm::vec3(0.0f);
	lure.remainingSeconds = 5.0f;
	expectTrue(fieldLureAttracts(PokemonSpecies::Eevee,
	                            glm::vec3(10.0f, 8.0f, 0.0f), 0.2f, lure),
	           "a calm Eevee inside the horizontal scent radius is attracted");
	expectTrue(!fieldLureAttracts(PokemonSpecies::Bulbasaur,
	                             glm::vec3(2.0f), 0.0f, lure) &&
	               !fieldLureAttracts(PokemonSpecies::Eevee,
	                                  glm::vec3(2.0f), 0.5f, lure) &&
	               !fieldLureAttracts(PokemonSpecies::Eevee,
	                                  glm::vec3(19.0f, 0.0f, 0.0f), 0.0f, lure),
	           "other species, alerted Eevee, and distant Eevee ignore the lure");
}

void testLureBonusAndExpiryUseWorldDistanceAndSimulationTime()
{
	FieldLureState lure;
	lure.active = true;
	lure.position = glm::vec3(0.0f, 20.0f, 0.0f);
	lure.remainingSeconds = 0.5f;
	expectTrue(fieldLureCaptureBonusApplies(glm::vec3(3.0f, 0.0f, 0.0f), lure) &&
	               !fieldLureCaptureBonusApplies(glm::vec3(4.0f, 20.0f, 0.0f), lure),
	           "capture bonus uses the configured horizontal lure radius");
	expectTrue(!updateFieldLure(lure, 0.25f) && lure.active &&
	               std::fabs(lure.remainingSeconds - 0.25f) < 0.0001f,
	           "partial fixed steps advance an active lure without expiring it");
	expectTrue(updateFieldLure(lure, 0.25f) && !lure.active &&
	               lure.remainingSeconds == 0.0f,
	           "the exact final fixed step expires and clears the lure");
}
}

int main()
{
	testDeploymentRequiresUnlockInventoryAndGrounding();
	testOnlyCalmNearbyEeveeFollowTheLure();
	testLureBonusAndExpiryUseWorldDistanceAndSimulationTime();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Field lure tests passed" << std::endl;
	return 0;
}
