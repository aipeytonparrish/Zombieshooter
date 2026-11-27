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

/** One item for sale in today's market. */
struct FZSMarketOffer
{
	FName ItemId;
	int32 Price = 0;
	int32 FullPrice = 0;
	bool bDeal = false;      // deal of the day (discounted)
	bool bStarter = false;   // always in stock
};

/**
 * The player profile for the whole session: XP / level (CurveTable CT_ZSLevels), cash, owned weapons (each with its
 * own attachments), 5 loadout presets, the character look and lifetime stats, plus the risk rules:
 *  - deploy (BeginRun) saves an "active run" flag, so force-quitting mid-run counts as a fail on the next launch;
 *  - win: keep the loadout, bank all run XP / cash plus a win bonus;
 *  - fail (death / abandon / force quit): the deployed weapons (and their attachments) are removed from the inventory
 *    and the run's XP and cash are not banked. The character look is never lost.
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
	static constexpr int32 NumLoadouts = 5;
	static constexpr int32 MaxWeapons = 24;
	static constexpr float SellFraction = 0.4f;
	static constexpr float DealDiscount = 0.25f;
	/** Rotating (non-starter) weapons in stock each day. */
	static constexpr int32 DailyWeaponSlots = 3;

	// ---- Queries ----
	int32 GetLevel() const;
	int32 GetLevelXP() const;
	int32 GetXPToNextLevel() const { return XPToNext(GetLevel()); }
	int64 GetCash() const;
	int32 XPToNext(int32 Level) const;
	const TArray<FZSInventoryWeapon>& GetWeapons() const;
	const FZSInventoryWeapon* FindWeapon(const FGuid& Id) const;
	const FZSLifetimeStats& GetStats() const;

	// ---- Loadouts ----
	int32 GetActiveLoadoutIndex() const;
	FZSLoadout GetLoadout(int32 Index) const;
	void SetActiveLoadout(int32 Index);
	/** Puts a weapon in the preset's primary or sidearm slot (decided by the weapon). */
	bool AssignToLoadout(int32 Index, const FGuid& WeaponId);
	void ClearLoadoutSlot(int32 Index, bool bPrimary);
	/** Display names of a preset's weapons (-1 = the active preset), primary first. */
	TArray<FText> GetLoadoutNames(int32 Index = -1) const;
	/** "M4 Tactical +2" style name of an owned weapon. */
	FText DescribeWeapon(const FGuid& WeaponId) const;

	// ---- Market / armory ----
	static int32 MarketDay();
	static FTimespan TimeToMarketRefresh();
	TArray<FZSMarketOffer> GetMarketOffers() const;
	/** Buys a weapon from today's market (adds it to the active preset if that slot is empty). */
	bool BuyWeapon(FName ItemId, FText& OutMessage);
	int32 GetSellPrice(const FGuid& WeaponId) const;
	bool SellWeapon(const FGuid& WeaponId, FText& OutMessage);
	/** Why this attachment can't be fitted (empty = it can). */
	FText CanFitAttachment(const FGuid& WeaponId, FName AttachmentId) const;
	bool BuyAttachment(const FGuid& WeaponId, FName AttachmentId, FText& OutMessage);
	/** Removing scraps the attachment (no refund). */
	bool RemoveAttachment(const FGuid& WeaponId, FName AttachmentId);

	// ---- Character ----
	const FZSCharacterLook& GetLook() const;
	void SetLook(const FZSCharacterLook& Look);

	// ---- Runs ----
	/** Deploy: validates the active preset, saves the active-run flag. Returns the deployed weapons (primary first). */
	TArray<FZSInventoryWeapon> BeginRun();
	bool IsRunActive() const;
	/** Weapons currently deployed (empty outside a run). */
	TArray<FZSInventoryWeapon> GetDeployedWeapons() const;
	TArray<FName> GetDeployedItemIds() const;
	/** Display names of what a fail would cost right now. */
	TArray<FText> GetAtRiskNames() const;

	/** Apply the win / lose rules and save. */
	FZSRunResult EndRun(EZSRunOutcome Outcome, const FZSRunStats& Stats);

	/** Set when the previous session ended mid-run (force quit); shown once, then cleared. */
	bool ConsumeForceQuitNotice(FZSRunResult& OutResult);

	/** Generic per-key counters other systems persist here (missions etc.). */
	UZSSaveGame* GetSave() const { return Profile; }

	// ---- Debug ----
	void ResetProfile();
	void GiveCash(int64 Amount);
	void GiveXP(int32 Amount);

	void Save();

private:
	void Load();
	void EnsureStarterWeapons();
	void EnsureLoadouts();
	FGuid AddWeapon(FName ItemId);
	FZSInventoryWeapon* FindWeaponMutable(const FGuid& Id);
	FZSLoadout& ActiveLoadoutRef();
	/** Drops references to weapons that no longer exist from every preset. */
	void CleanLoadouts();
	/** Fills a preset's empty slots with an owned weapon of the right kind. */
	void FillEmptySlots(FZSLoadout& L);
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
