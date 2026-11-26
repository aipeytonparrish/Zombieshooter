#pragma once

#include "CoreMinimal.h"
#include "Core/ZSGameMode_Sandbox.h"
#include "ZSGameMode_Survival.generated.h"

class UZSRoundManagerComponent;

/**
 * Survival match (Milestone 5): rounds, out-of-view spawns, pickups, unlimited ammo crates, game over.
 * Use it as a level's GameMode Override, or add "?game=Survival" to the map URL.
 * Milestone 6 adds the profile / XP / cash / results screen on top of the game-over flow.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSGameMode_Survival : public AZSGameMode_Sandbox
{
	GENERATED_BODY()

public:
	AZSGameMode_Survival();

	UZSRoundManagerComponent* GetRoundManager() const { return RoundManager; }

	/** Seconds the game-over screen shows before the run restarts. */
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Survival")
	float RestartDelay = 8.f;

	virtual bool HandlePlayerDeath(class AZSPlayerCharacter* Player) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Restart the run now (also triggered from the game-over screen). */
	void RestartRun();

protected:
	UPROPERTY(VisibleAnywhere, Category = "ZS|Survival")
	TObjectPtr<UZSRoundManagerComponent> RoundManager;

private:
	float RestartTimer = -1.f;
};
