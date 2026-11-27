#include "Profile/ZSProfileSubsystem.h"
#include "Profile/ZSSaveGame.h"
#include "Weapons/ZSWeaponCatalog.h"
#include "Engine/CurveTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Zombieshooter.h"

const TCHAR* UZSProfileSubsystem::SlotName = TEXT("Profile0");

UZSProfileSubsystem* UZSProfileSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UZSProfileSubsystem>() : nullptr;
}

void UZSProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LevelCurves = LoadObject<UCurveTable>(nullptr, TEXT("/Game/ZS/Data/CT_ZSLevels.CT_ZSLevels"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	Load();
}

void UZSProfileSubsystem::Load()
{
	Profile = Cast<UZSSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Profile)
	{
		Profile = Cast<UZSSaveGame>(UGameplayStatics::CreateSaveGameObject(UZSSaveGame::StaticClass()));
		EnsureStarterWeapons();
		UE_LOG(LogZS, Log, TEXT("New profile created."));
		Save();
		return;
	}

	// The previous session ended mid-run: that is a fail (force quit).
	if (Profile->bRunActive)
	{
		ForceQuitResult = FZSRunResult();
		ForceQuitResult.Outcome = EZSRunOutcome::ForceQuit;
		ForceQuitResult.LevelBefore = ForceQuitResult.LevelAfter = Profile->Level;
		ForceQuitResult.LostItems = LoseDeployedWeapons();
		ForceQuitResult.LevelXP = Profile->LevelXP;
		ForceQuitResult.LevelXPNeeded = XPToNext(Profile->Level);
		Profile->bRunActive = false;
		Profile->Stats.Runs++;
		bHasForceQuitNotice = true;
		UE_LOG(LogZS, Warning, TEXT("Previous run was not finished (force quit) - deployed gear lost."));
		Save();
	}
	UE_LOG(LogZS, Log, TEXT("Profile loaded: level %d (%d XP), $%lld, %d weapons."), Profile->Level, Profile->LevelXP, Profile->Cash, Profile->Weapons.Num());
}

void UZSProfileSubsystem::Save()
{
	if (Profile)
	{
		UGameplayStatics::SaveGameToSlot(Profile, SlotName, 0);
	}
}

// ---- Queries --------------------------------------------------------------------------------

int32 UZSProfileSubsystem::GetLevel() const { return Profile ? Profile->Level : 1; }
int32 UZSProfileSubsystem::GetLevelXP() const { return Profile ? Profile->LevelXP : 0; }
int64 UZSProfileSubsystem::GetCash() const { return Profile ? Profile->Cash : 0; }
const FZSLifetimeStats& UZSProfileSubsystem::GetStats() const { static FZSLifetimeStats Empty; return Profile ? Profile->Stats : Empty; }

const TArray<FZSInventoryWeapon>& UZSProfileSubsystem::GetWeapons() const
{
	static TArray<FZSInventoryWeapon> Empty;
	return Profile ? Profile->Weapons : Empty;
}

int32 UZSProfileSubsystem::XPToNext(int32 Level) const
{
	const float L = static_cast<float>(FMath::Clamp(Level, 1, MaxLevel));
	float Needed = 800.f + 250.f * FMath::Pow(L, 1.25f);   // same formula Data/ZSLevels.csv was generated from
	if (LevelCurves)
	{
		static const FString Ctx(TEXT("ZSLevels"));
		if (const FRealCurve* Curve = LevelCurves->FindCurve(TEXT("XPToNext"), Ctx, false))
		{
			Needed = Curve->Eval(L, Needed);
		}
	}
	return FMath::Max(1, FMath::RoundToInt(Needed));
}

const FZSInventoryWeapon* UZSProfileSubsystem::FindWeapon(const FGuid& Id) const
{
	return Profile ? Profile->Weapons.FindByPredicate([&Id](const FZSInventoryWeapon& W) { return W.InstanceId == Id; }) : nullptr;
}

// ---- Inventory ------------------------------------------------------------------------------

FGuid UZSProfileSubsystem::AddWeapon(FName ItemId)
{
	FZSInventoryWeapon& W = Profile->Weapons.AddDefaulted_GetRef();
	W.InstanceId = FGuid::NewGuid();
	W.ItemId = ItemId;
	return W.InstanceId;
}

void UZSProfileSubsystem::EnsureStarterWeapons()
{
	for (const ZSWeaponCatalog::FEntry& E : ZSWeaponCatalog::All())
	{
		if (!E.bStarter)
		{
			continue;
		}
		const bool bOwned = Profile->Weapons.ContainsByPredicate([&E](const FZSInventoryWeapon& W) { return W.ItemId == E.ItemId; });
		if (!bOwned)
		{
			AddWeapon(E.ItemId);
		}
	}
}

TArray<FText> UZSProfileSubsystem::LoseDeployedWeapons()
{
	TArray<FText> Names;
	for (const FGuid& Id : Profile->DeployedWeapons)
	{
		const int32 Index = Profile->Weapons.IndexOfByPredicate([&Id](const FZSInventoryWeapon& W) { return W.InstanceId == Id; });
		if (Index != INDEX_NONE)
		{
			Names.Add(ZSWeaponCatalog::DisplayName(Profile->Weapons[Index].ItemId));
			Profile->Weapons.RemoveAt(Index);
			Profile->Stats.WeaponsLost++;
		}
	}
	Profile->DeployedWeapons.Reset();
	if (!FindWeapon(Profile->PrimaryWeapon)) { Profile->PrimaryWeapon.Invalidate(); }
	if (!FindWeapon(Profile->SecondaryWeapon)) { Profile->SecondaryWeapon.Invalidate(); }
	return Names;
}

// ---- Runs -----------------------------------------------------------------------------------

TArray<FName> UZSProfileSubsystem::BeginRun()
{
	if (!Profile)
	{
		return {};
	}
	if (bAutoRestockStarters)
	{
		EnsureStarterWeapons();
	}

	// Fill empty / invalid slots with the first owned weapon of the right kind.
	auto PickSlot = [this](FGuid& Slot, bool bPrimary)
	{
		if (const FZSInventoryWeapon* W = FindWeapon(Slot))
		{
			const ZSWeaponCatalog::FEntry* E = ZSWeaponCatalog::Find(W->ItemId);
			if (E && E->bPrimary == bPrimary)
			{
				return;
			}
		}
		Slot.Invalidate();
		for (const FZSInventoryWeapon& W : Profile->Weapons)
		{
			const ZSWeaponCatalog::FEntry* E = ZSWeaponCatalog::Find(W.ItemId);
			if (E && E->bPrimary == bPrimary)
			{
				Slot = W.InstanceId;
				return;
			}
		}
	};
	PickSlot(Profile->PrimaryWeapon, true);
	PickSlot(Profile->SecondaryWeapon, false);

	Profile->DeployedWeapons.Reset();
	if (Profile->PrimaryWeapon.IsValid()) { Profile->DeployedWeapons.Add(Profile->PrimaryWeapon); }
	if (Profile->SecondaryWeapon.IsValid()) { Profile->DeployedWeapons.Add(Profile->SecondaryWeapon); }
	Profile->bRunActive = true;
	Save();
	return GetDeployedItemIds();
}

bool UZSProfileSubsystem::IsRunActive() const
{
	return Profile && Profile->bRunActive;
}

TArray<FName> UZSProfileSubsystem::GetDeployedItemIds() const
{
	TArray<FName> Ids;
	if (Profile)
	{
		for (const FGuid& Id : Profile->DeployedWeapons)
		{
			if (const FZSInventoryWeapon* W = FindWeapon(Id))
			{
				Ids.Add(W->ItemId);
			}
		}
	}
	return Ids;
}

TArray<FText> UZSProfileSubsystem::GetAtRiskNames() const
{
	TArray<FText> Names;
	for (const FName Id : GetDeployedItemIds())
	{
		Names.Add(ZSWeaponCatalog::DisplayName(Id));
	}
	return Names;
}

int32 UZSProfileSubsystem::AddXP(int32 Amount)
{
	int32 Gained = 0;
	Profile->TotalXP += Amount;
	Profile->LevelXP += Amount;
	while (Profile->Level < MaxLevel && Profile->LevelXP >= XPToNext(Profile->Level))
	{
		Profile->LevelXP -= XPToNext(Profile->Level);
		Profile->Level++;
		Gained++;
	}
	if (Profile->Level >= MaxLevel)
	{
		Profile->LevelXP = FMath::Min(Profile->LevelXP, XPToNext(MaxLevel));   // prestige arrives in Milestone 13
	}
	return Gained;
}

FZSRunResult UZSProfileSubsystem::EndRun(EZSRunOutcome Outcome, const FZSRunStats& Stats)
{
	FZSRunResult R;
	if (!Profile)
	{
		return R;
	}
	R.Outcome = Outcome;
	R.RoundReached = Stats.RoundReached;
	R.Kills = Stats.Kills;
	R.Headshots = Stats.Headshots;
	R.TasksCompleted = Stats.TasksCompleted;
	R.RunTime = Stats.RunTime;
	R.XPEarned = Stats.XPEarned;
	R.CashEarned = Stats.CashEarned;
	R.LevelBefore = Profile->Level;

	if (Outcome == EZSRunOutcome::Won)
	{
		R.XPBonus = WinBonusXP;
		R.CashBonus = WinBonusCash;
		R.XPKept = R.XPEarned + R.XPBonus;
		R.CashKept = R.CashEarned + R.CashBonus;
		AddXP(R.XPKept);
		Profile->Cash += R.CashKept;
		Profile->Stats.Wins++;
		Profile->DeployedWeapons.Reset();
	}
	else
	{
		R.LostItems = LoseDeployedWeapons();
	}

	R.LevelAfter = Profile->Level;
	R.LevelXP = Profile->LevelXP;
	R.LevelXPNeeded = XPToNext(Profile->Level);
	for (int32 L = R.LevelBefore + 1; L <= R.LevelAfter; ++L)
	{
		R.Unlocks.Add(FText::Format(NSLOCTEXT("ZS", "LevelUnlock", "Level {0} reached"), FText::AsNumber(L)));
	}

	FZSLifetimeStats& S = Profile->Stats;
	S.Runs++;
	S.Kills += Stats.Kills;
	S.Headshots += Stats.Headshots;
	S.HighestRound = FMath::Max(S.HighestRound, Stats.RoundReached);
	S.PlaySeconds += Stats.RunTime;

	Profile->bRunActive = false;
	Save();
	UE_LOG(LogZS, Log, TEXT("Run ended (%d): XP earned %d kept %d, cash earned %d kept %d, level %d -> %d, lost %d weapon(s)."),
		(int32)Outcome, R.XPEarned, R.XPKept, R.CashEarned, R.CashKept, R.LevelBefore, R.LevelAfter, R.LostItems.Num());
	return R;
}

bool UZSProfileSubsystem::ConsumeForceQuitNotice(FZSRunResult& OutResult)
{
	if (!bHasForceQuitNotice)
	{
		return false;
	}
	bHasForceQuitNotice = false;
	OutResult = ForceQuitResult;
	return true;
}

// ---- Debug ----------------------------------------------------------------------------------

void UZSProfileSubsystem::ResetProfile()
{
	Profile = Cast<UZSSaveGame>(UGameplayStatics::CreateSaveGameObject(UZSSaveGame::StaticClass()));
	EnsureStarterWeapons();
	Save();
}

void UZSProfileSubsystem::GiveCash(int64 Amount)
{
	if (Profile) { Profile->Cash = FMath::Max<int64>(0, Profile->Cash + Amount); Save(); }
}

void UZSProfileSubsystem::GiveXP(int32 Amount)
{
	if (Profile) { AddXP(FMath::Max(0, Amount)); Save(); }
}
