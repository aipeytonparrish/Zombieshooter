#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ZSGameMode_Sandbox.generated.h"

/**
 * Free-roam game mode for test maps (movement course, later weapon range).
 * Set as the project-wide default game mode in Config/DefaultEngine.ini, so any new level
 * spawns the ZS player with full movement and input out of the box.
 * The survival match mode (AZSGameMode_Survival) arrives in Milestone 5.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSGameMode_Sandbox : public AGameModeBase
{
	GENERATED_BODY()

public:
	AZSGameMode_Sandbox();
};
