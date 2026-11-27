#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Profile/ZSProfileTypes.h"
#include "ZSSaveGame.generated.h"

/**
 * The player profile on disk (Saved/SaveGames/Profile0.sav; Steam Cloud later). Written after every run and every
 * purchase. Fields for later milestones (loadouts, character, missions, market, unlocked levels, prestige) are
 * reserved here so old saves keep loading as features arrive.
 */
UCLASS()
class ZOMBIESHOOTER_API UZSSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY(SaveGame) int32 Version = CurrentVersion;

	// ---- Progression ----
	UPROPERTY(SaveGame) int32 Level = 1;
	/** XP into the current level. */
	UPROPERTY(SaveGame) int32 LevelXP = 0;
	UPROPERTY(SaveGame) int64 TotalXP = 0;
	UPROPERTY(SaveGame) int32 Prestige = 0;
	UPROPERTY(SaveGame) int64 Cash = 0;

	// ---- Inventory / loadout ----
	UPROPERTY(SaveGame) TArray<FZSInventoryWeapon> Weapons;
	/** Instance ids of the deployed primary / secondary (Milestone 8 turns this into 5 loadout presets). */
	UPROPERTY(SaveGame) FGuid PrimaryWeapon;
	UPROPERTY(SaveGame) FGuid SecondaryWeapon;

	// ---- Force-quit guard: set at deploy, cleared at the end of a run ----
	UPROPERTY(SaveGame) bool bRunActive = false;
	UPROPERTY(SaveGame) TArray<FGuid> DeployedWeapons;

	// ---- Stats ----
	UPROPERTY(SaveGame) FZSLifetimeStats Stats;

	// ---- Reserved for later milestones ----
	UPROPERTY(SaveGame) TArray<FName> UnlockedLevels;
	UPROPERTY(SaveGame) TMap<FName, int32> MissionProgress;
};
