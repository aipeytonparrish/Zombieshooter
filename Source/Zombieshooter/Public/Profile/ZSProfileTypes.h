#pragma once

#include "CoreMinimal.h"
#include "ZSProfileTypes.generated.h"

/** One owned weapon. Each instance carries its own attachments (per-gun attachments arrive in Milestone 8). */
USTRUCT(BlueprintType)
struct FZSInventoryWeapon
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Inventory") FGuid InstanceId;
	/** Catalog id, e.g. "AR_M4" (see UZSWeaponCatalog). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Inventory") FName ItemId;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Inventory") TArray<FName> Attachments;
};

/** One of the 5 loadout presets: a primary and a sidearm (instance ids into the inventory). */
USTRUCT(BlueprintType)
struct FZSLoadout
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Loadout") FString Name;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Loadout") FGuid Primary;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Loadout") FGuid Secondary;
};

/** The player's look (Milestone 9). Never lost on a failed run. */
USTRUCT(BlueprintType)
struct FZSCharacterLook
{
	GENERATED_BODY()

	/** Survivor body 0..N-1 (ZSCharacterCatalog). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Character") int32 Body = 0;
	/** First-person arms / gloves set. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Character") int32 Arms = 0;
	/** Index into the tint palette (0 = none). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Character") int32 Tint = 0;
	/** Arms follow the body's outfit automatically. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Character") bool bMatchArms = true;
};

UENUM(BlueprintType)
enum class EZSRunOutcome : uint8
{
	None,
	Won,
	Died,
	Abandoned,
	ForceQuit   // detected on the next launch
};

/** Everything the results screen shows. */
USTRUCT(BlueprintType)
struct FZSRunResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Run") EZSRunOutcome Outcome = EZSRunOutcome::None;
	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 RoundReached = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 Kills = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 Headshots = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 TasksCompleted = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Run") float RunTime = 0.f;

	/** Earned during the run (before win/lose rules). */
	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 XPEarned = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 CashEarned = 0;
	/** Win bonus (0 on a fail). */
	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 XPBonus = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 CashBonus = 0;
	/** What actually went into the profile. */
	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 XPKept = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 CashKept = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 LevelBefore = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 LevelAfter = 1;
	/** XP into the current level / needed for the next, after the run. */
	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 LevelXP = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Run") int32 LevelXPNeeded = 1;

	/** Display names of weapons removed from the inventory by a fail. */
	UPROPERTY(BlueprintReadOnly, Category = "Run") TArray<FText> LostItems;
	/** Display names of things unlocked by level-ups (market items arrive in Milestone 8). */
	UPROPERTY(BlueprintReadOnly, Category = "Run") TArray<FText> Unlocks;
};

USTRUCT(BlueprintType)
struct FZSLifetimeStats
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Stats") int32 Runs = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Stats") int32 Wins = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Stats") int32 Kills = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Stats") int32 Headshots = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Stats") int32 HighestRound = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Stats") float PlaySeconds = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Stats") int32 WeaponsLost = 0;
};
