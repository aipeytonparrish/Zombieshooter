#pragma once

#include "CoreMinimal.h"
#include "Bunker/ZSStation.h"

class AZSPlayerController;

/** Builds the menu pages for each bunker station. */
namespace ZSBunkerMenus
{
	ZOMBIESHOOTER_API void Open(AZSPlayerController* PC, EZSStationType Type);

	// Individual stations (also callable from console / tests).
	ZOMBIESHOOTER_API void OpenMapTable(AZSPlayerController* PC);
	ZOMBIESHOOTER_API void OpenArmory(AZSPlayerController* PC);
	ZOMBIESHOOTER_API void OpenMarket(AZSPlayerController* PC);
	ZOMBIESHOOTER_API void OpenCharacter(AZSPlayerController* PC);
	ZOMBIESHOOTER_API void OpenMissions(AZSPlayerController* PC);

	/** Leave the bunker for a level (map URL from ZSLevelCatalog). */
	ZOMBIESHOOTER_API void Deploy(AZSPlayerController* PC, FName LevelId);
}
