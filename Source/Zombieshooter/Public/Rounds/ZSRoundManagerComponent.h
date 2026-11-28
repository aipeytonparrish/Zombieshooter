#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZSRoundManagerComponent.generated.h"

class AZSZombieBase;
class AZSSpawnPoint;
class AZSGameState_Survival;
class UCurveTable;
class USoundBase;

/** One round's tuning, read from the CurveTable (or the built-in formulas if the table is missing). */
USTRUCT(BlueprintType)
struct FZSRoundTuning
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Round") int32 ZombieCount = 8;
	UPROPERTY(BlueprintReadOnly, Category = "Round") float HealthMultiplier = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Round") float SpeedMultiplier = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Round") float RunnerChance = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Round") float SpawnInterval = 2.f;
	UPROPERTY(BlueprintReadOnly, Category = "Round") int32 MaxAlive = 10;
};

/**
 * Runs the survival loop on the game mode: pre-game -> round N (spawn budget, out-of-view spawns, live cap) ->
 * break with stinger -> round N+1 ... until the player dies. Round scaling comes from the CurveTable
 * /Game/ZS/Data/CT_ZSRounds (rows ZombieCount, HealthMultiplier, SpeedMultiplier, RunnerChance, SpawnInterval,
 * MaxAlive keyed by round), generated from Data/ZSRounds.csv. The round-20 boss arrives in Milestone 12.
 */
UCLASS(ClassGroup = (ZS), meta = (BlueprintSpawnableComponent))
class ZOMBIESHOOTER_API UZSRoundManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZSRoundManagerComponent();

	UPROPERTY(EditAnywhere, Category = "ZS|Rounds")
	TSoftObjectPtr<UCurveTable> RoundCurves;

	UPROPERTY(EditAnywhere, Category = "ZS|Rounds", meta = (ForceUnits = "s"))
	float PreGameDuration = 8.f;

	UPROPERTY(EditAnywhere, Category = "ZS|Rounds", meta = (ForceUnits = "s"))
	float BreakDuration = 20.f;

	/** Hard ceiling on live zombies whatever the curve says (performance). */
	UPROPERTY(EditAnywhere, Category = "ZS|Rounds")
	int32 MaxAliveCeiling = 32;

	UPROPERTY(EditAnywhere, Category = "ZS|Spawning", meta = (ForceUnits = "cm"))
	float MinSpawnDistance = 900.f;

	UPROPERTY(EditAnywhere, Category = "ZS|Spawning", meta = (ForceUnits = "cm"))
	float MaxSpawnDistance = 3500.f;

	/** Chance a killed zombie drops a pickup, and the split between health and armor. */
	UPROPERTY(EditAnywhere, Category = "ZS|Pickups")
	float PickupDropChance = 0.07f;

	/** Chance per kill of a power-up (Overkill, One Shot, Payday, Scorched Earth). */
	UPROPERTY(EditAnywhere, Category = "ZS|Rounds")
	float PowerUpDropChance = 0.022f;

	UPROPERTY(EditAnywhere, Category = "ZS|Pickups")
	float ArmorDropShare = 0.5f;

	UPROPERTY(EditAnywhere, Category = "ZS|Pickups")
	int32 MaxPickupsAlive = 6;

	UPROPERTY(EditAnywhere, Category = "ZS|Audio")
	TObjectPtr<USoundBase> RoundStartSound;

	UPROPERTY(EditAnywhere, Category = "ZS|Audio")
	TObjectPtr<USoundBase> RoundEndSound;

	/** Tuning for a round (1-based). */
	FZSRoundTuning GetTuning(int32 Round) const;

	/** Stop everything: the player died. */
	void EndRun();

	/** Adds zombies to the current round (boss roars). */
	void SpawnExtra(int32 Count);

	/** True during the boss round (the round ends when the boss dies, not by count). */
	bool IsBossRound() const { return bBossRound; }

	/** Debug: jump straight to a round (kills the current zombies). */
	void SkipToRound(int32 Round);

	/** Zones (AZSSpawnPoint::Zone) opened by bought doors. */
	void OpenZone(FName Zone);
	bool IsZoneOpen(FName Zone) const { return Zone.IsNone() || OpenZones.Contains(Zone); }

	/** A zombie that has made no progress towards the player for a while (stuck on debris, lost behind a wall) is
	 *  moved to a fresh spawn point out of view. Returns false if it was left where it is (no point, or visible). */
	bool RelocateStraggler(AZSZombieBase* Zombie, bool bForce = false);

	/** Horde surges: from this round on, every SurgeInterval seconds of a round a burst of zombies arrives at once. */
	UPROPERTY(EditAnywhere, Category = "ZS|Rounds")
	int32 SurgeFromRound = 3;
	UPROPERTY(EditAnywhere, Category = "ZS|Rounds", meta = (ForceUnits = "s"))
	float SurgeInterval = 30.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	void StartRound(int32 Round);
	void SpawnOne();
	bool PickSpawnLocation(FVector& OutLocation, FRotator& OutRotation, bool& bOutAllowRunners) const;
	bool IsVisibleToPlayer(const FVector& Point) const;
	void HandleZombieKilled(AZSZombieBase* Zombie, AController* Killer, bool bHeadshot);
	void MaybeDropPickup(const FVector& Location);
	void PlayStinger(USoundBase* Sound, float Volume, float Pitch) const;

	AZSGameState_Survival* GetState() const;
	APawn* GetPlayerPawn() const;

	UPROPERTY(Transient)
	TObjectPtr<UCurveTable> LoadedCurves;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AZSSpawnPoint>> SpawnPoints;

	FZSRoundTuning Tuning;
	TSet<FName> OpenZones;
	/** The spawn point PickSpawnLocation chose last (for its barricade). */
	mutable TWeakObjectPtr<const class AZSSpawnPoint> LastPickedPoint;
	float SurgeTimer = 0.f;
	int32 SurgeLeft = 0;
	void StartSurge();
	int32 ToSpawn = 0;
	bool bBossRound = false;
	void SpawnBoss();
	int32 Alive = 0;
public:
	int32 GetAliveCount() const { return Alive; }
private:
	float SpawnTimer = 0.f;
	float PhaseTimer = 0.f;
	bool bRunOver = false;
};
