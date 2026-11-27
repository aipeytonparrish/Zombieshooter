#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Profile/ZSProfileTypes.h"
#include "ZSProfileSubsystem.generated.h"

class UZSSaveGame;
class UCurveTable;

/** What a run earned, handed to EndRun. */
struct FZSRunStats
{
	int32 RoundReached = 0;
	int32 Kills = 0;
	int32 Headshots = 0;
	int32 TasksCompleted = 0;
	float RunTime = 0.f;
	int32 XPEarned = 0;
	int32 CashEarned = 0;
};

/**
 * The player profile for the whole session: XP / level (CurveTable CT_ZSLevels), cash, owned weapons, the deployed
 * loadout and lifetime stats, plus the risk rules:
 *  - deploy (BeginRun) saves an "active run" flag, so force-quitting mid-run counts as a fail on the next launch;
 *  - win: keep the loadout, bank all run XP / cash plus a win bonus;
 *  - fail (death / abandon / force quit): the deployed weapons are removed from the inventory and the run's XP and
 *    cash are not banked. Outfits (Milestone 9) are never lost.
 */
UCLASS()
class ZOMBIESHOOTER_API UZSProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UZSProfileSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// ---- Tuning ----
	static constexpr int32 MaxLevel = 55;
	static constexpr int32 WinBonusXP = 2500;
	static constexpr int32 WinBonusCash = 1000;
	/** Until the market (Milestone 8) exists, missing starter weapons are re-issued at deploy so a fail can't soft-lock. */
	static constexpr bool bAutoRestockStarters = true;

	// ---- Queries ----
	int32 GetLevel() const;
	int32 GetLevelXP() const;
	int32 GetXPToNextLevel() const { return XPToNext(GetLevel()); }
	int64 GetCash() const;
	int32 XPToNext(int32 Level) const;
	const TArray<FZSInventoryWeapon>& GetWeapons() const;
	const FZSLifetimeStats& GetStats() const;

	// ---- Runs ----
	/** Deploy: validates the loadout, saves the active-run flag. Returns the deployed item ids (primary first). */
	TArray<FName> BeginRun();
	bool IsRunActive() const;
	/** Item ids currently deployed (empty outside a run). */
	TArray<FName> GetDeployedItemIds() const;
	/** Display names of what a fail would cost right now. */
	TArray<FText> GetAtRiskNames() const;

	/** Apply the win / lose rules and save. */
	FZSRunResult EndRun(EZSRunOutcome Outcome, const FZSRunStats& Stats);

	/** Set when the previous session ended mid-run (force quit); shown once, then cleared. */
	bool ConsumeForceQuitNotice(FZSRunResult& OutResult);

	// ---- Debug ----
	void ResetProfile();
	void GiveCash(int64 Amount);
	void GiveXP(int32 Amount);

	void Save();

private:
	void Load();
	void EnsureStarterWeapons();
	FGuid AddWeapon(FName ItemId);
	const FZSInventoryWeapon* FindWeapon(const FGuid& Id) const;
	/** Adds XP, handling level-ups. Returns levels gained. */
	int32 AddXP(int32 Amount);
	/** Removes the deployed weapons; returns their names. */
	TArray<FText> LoseDeployedWeapons();

	UPROPERTY(Transient)
	TObjectPtr<UZSSaveGame> Profile;

	UPROPERTY(Transient)
	TObjectPtr<UCurveTable> LevelCurves;

	bool bHasForceQuitNotice = false;
	FZSRunResult ForceQuitResult;

	static const TCHAR* SlotName;
};
