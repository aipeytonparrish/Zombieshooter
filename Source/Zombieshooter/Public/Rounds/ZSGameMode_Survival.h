#pragma once

#include "CoreMinimal.h"
#include "Core/ZSGameMode_Sandbox.h"
#include "Profile/ZSProfileTypes.h"
#include "ZSGameMode_Survival.generated.h"

class UZSRoundManagerComponent;
class AZSWeaponBase;
class AZSZombieBase;

/**
 * Survival match: rounds (UZSRoundManagerComponent), the run economy (XP / cash per kill, headshot, multi-kill and
 * round) and the win / lose rules through UZSProfileSubsystem, ending in the results screen.
 * Use it as a level's GameMode Override, or add "?game=Survival" to the map URL.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSGameMode_Survival : public AZSGameMode_Sandbox
{
	GENERATED_BODY()

public:
	AZSGameMode_Survival();

	UZSRoundManagerComponent* GetRoundManager() const { return RoundManager; }

	// ---- Economy tuning ----
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Economy") int32 KillXP = 10;
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Economy") int32 KillCash = 10;
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Economy") int32 RunnerBonus = 5;
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Economy") int32 HeadshotBonus = 5;
	/** Per extra kill within MultiKillWindow of the previous one. */
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Economy") int32 MultiKillXP = 10;
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Economy") float MultiKillWindow = 1.2f;
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Economy") int32 RoundXPBase = 100;
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Economy") int32 RoundXPPerRound = 25;
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Economy") int32 RoundCashPerRound = 50;

	/** Seconds the results screen waits before continuing on its own. */
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Survival") float ResultsAutoContinue = 60.f;

	/** Weapons to give the player: the deployed loadout (deploys the run the first time it's asked). */
	TArray<TSubclassOf<AZSWeaponBase>> GetDeployedWeaponClasses();

	// ---- Called by the round manager ----
	void RewardKill(AZSZombieBase* Zombie, bool bHeadshot);
	void OnRoundCleared(int32 Round);

	// ---- Run end ----
	virtual bool HandlePlayerDeath(class AZSPlayerCharacter* Player) override;
	/** Pause menu: give up (counts as a fail). */
	void AbandonRun();
	void EndRun(EZSRunOutcome Outcome);
	/** Continue from the results screen (restarts the map; returns to the bunker in Milestone 7). */
	void RestartRun();
	bool IsRunOver() const { return bRunEnded; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "ZS|Survival")
	TObjectPtr<UZSRoundManagerComponent> RoundManager;

private:
	void EnsureDeployed();
	void AddReward(int32 XP, int32 Cash, const FText& Label, bool bBig = false);

	bool bDeployed = false;
	bool bRunEnded = false;
	float ResultsTimer = -1.f;
	float LastKillTime = -10.f;
	int32 KillStreak = 0;
	TArray<FName> DeployedIds;
};
