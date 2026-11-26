#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ZSGameMode_Sandbox.generated.h"

class APlayerStart;

/**
 * Free-roam game mode for test maps (movement course, later weapon range).
 * Set as the project-wide default game mode in Config/DefaultEngine.ini, so any new level
 * spawns the ZS player with full movement and input out of the box.
 * The survival match mode (AZSGameMode_Survival) arrives in Milestone 5.
 *
 * Add "?Course" to the map URL (e.g. "-game /Engine/Maps/Templates/Template_Default?Course") to spawn the
 * movement course and a start point at runtime when the level doesn't already contain one.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSGameMode_Sandbox : public AGameModeBase
{
	GENERATED_BODY()

public:
	AZSGameMode_Sandbox();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Return true if the mode handled the player's death (survival = game over). Sandbox returns false: respawn. */
	virtual bool HandlePlayerDeath(class AZSPlayerCharacter* Player) { return false; }

private:
	UPROPERTY(Transient)
	TObjectPtr<APlayerStart> CourseStart;
};
