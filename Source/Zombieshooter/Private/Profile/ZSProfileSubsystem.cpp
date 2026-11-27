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
		EnsureLoadouts();
		UE_LOG(LogZS, Log, TEXT("New profile created."));
		Save();
		return;
	}

	EnsureLoadouts();   // version 1 saves had a single loadout
	Profile->Version = UZSSaveGame::CurrentVersion;

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
	return (Profile && Id.IsValid()) ? Profile->Weapons.FindByPredicate([&Id](const FZSInventoryWeapon& W) { return W.InstanceId == Id; }) : nullptr;
}

FZSInventoryWeapon* UZSProfileSubsystem::FindWeaponMutable(const FGuid& Id)
{
	return const_cast<FZSInventoryWeapon*>(FindWeapon(Id));
}

const FZSCharacterLook& UZSProfileSubsystem::GetLook() const
{
	static FZSCharacterLook Default;
	return Profile ? Profile->Look : Default;
}

void UZSProfileSubsystem::SetLook(const FZSCharacterLook& Look)
{
	if (Profile) { Profile->Look = Look; Save(); }
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
		if (FindWeapon(Id))
		{
			Names.Add(DescribeWeapon(Id));
			Profile->Weapons.RemoveAll([&Id](const FZSInventoryWeapon& W) { return W.InstanceId == Id; });
			Profile->Stats.WeaponsLost++;
		}
	}
	Profile->DeployedWeapons.Reset();
	CleanLoadouts();
	FillEmptySlots(ActiveLoadoutRef());
	return Names;
}

// ---- Loadouts -------------------------------------------------------------------------------

void UZSProfileSubsystem::EnsureLoadouts()
{
	const bool bMigrate = Profile->Loadouts.Num() == 0;
	while (Profile->Loadouts.Num() < NumLoadouts)
	{
		FZSLoadout& L = Profile->Loadouts.AddDefaulted_GetRef();
		L.Name = FString::Printf(TEXT("LOADOUT %d"), Profile->Loadouts.Num());
	}
	Profile->Loadouts.SetNum(NumLoadouts);
	if (bMigrate)
	{
		Profile->Loadouts[0].Primary = Profile->PrimaryWeapon;
		Profile->Loadouts[0].Secondary = Profile->SecondaryWeapon;
		for (const FZSInventoryWeapon& W : Profile->Weapons)
		{
			const ZSWeaponCatalog::FEntry* E = ZSWeaponCatalog::Find(W.ItemId);
			if (!E)
			{
				continue;
			}
			FGuid& Slot = E->bPrimary ? Profile->Loadouts[0].Primary : Profile->Loadouts[0].Secondary;
			if (!FindWeapon(Slot)) { Slot = W.InstanceId; }
		}
	}
	Profile->ActiveLoadout = FMath::Clamp(Profile->ActiveLoadout, 0, NumLoadouts - 1);
	CleanLoadouts();
}

void UZSProfileSubsystem::CleanLoadouts()
{
	for (FZSLoadout& L : Profile->Loadouts)
	{
		if (!FindWeapon(L.Primary)) { L.Primary.Invalidate(); }
		if (!FindWeapon(L.Secondary)) { L.Secondary.Invalidate(); }
	}
}

FZSLoadout& UZSProfileSubsystem::ActiveLoadoutRef()
{
	if (Profile->Loadouts.Num() != NumLoadouts)
	{
		EnsureLoadouts();
	}
	return Profile->Loadouts[Profile->ActiveLoadout];
}

int32 UZSProfileSubsystem::GetActiveLoadoutIndex() const { return Profile ? Profile->ActiveLoadout : 0; }

FZSLoadout UZSProfileSubsystem::GetLoadout(int32 Index) const
{
	return (Profile && Profile->Loadouts.IsValidIndex(Index)) ? Profile->Loadouts[Index] : FZSLoadout();
}

void UZSProfileSubsystem::SetActiveLoadout(int32 Index)
{
	if (Profile && Profile->Loadouts.IsValidIndex(Index)) { Profile->ActiveLoadout = Index; Save(); }
}

bool UZSProfileSubsystem::AssignToLoadout(int32 Index, const FGuid& WeaponId)
{
	const FZSInventoryWeapon* W = FindWeapon(WeaponId);
	const ZSWeaponCatalog::FEntry* E = W ? ZSWeaponCatalog::Find(W->ItemId) : nullptr;
	if (!E || !Profile->Loadouts.IsValidIndex(Index))
	{
		return false;
	}
	FZSLoadout& L = Profile->Loadouts[Index];
	(E->bPrimary ? L.Primary : L.Secondary) = WeaponId;
	Save();
	return true;
}

void UZSProfileSubsystem::ClearLoadoutSlot(int32 Index, bool bPrimary)
{
	if (Profile && Profile->Loadouts.IsValidIndex(Index))
	{
		(bPrimary ? Profile->Loadouts[Index].Primary : Profile->Loadouts[Index].Secondary).Invalidate();
		Save();
	}
}

FText UZSProfileSubsystem::DescribeWeapon(const FGuid& WeaponId) const
{
	const FZSInventoryWeapon* W = FindWeapon(WeaponId);
	if (!W)
	{
		return NSLOCTEXT("ZS", "EmptySlot", "(empty)");
	}
	const FText Name = ZSWeaponCatalog::DisplayName(W->ItemId);
	return W->Attachments.Num() > 0
		? FText::Format(NSLOCTEXT("ZS", "WeaponPlus", "{0} +{1}"), Name, FText::AsNumber(W->Attachments.Num()))
		: Name;
}

TArray<FText> UZSProfileSubsystem::GetLoadoutNames(int32 Index) const
{
	TArray<FText> Names;
	if (!Profile)
	{
		return Names;
	}
	const FZSLoadout L = GetLoadout(Index < 0 ? Profile->ActiveLoadout : Index);
	if (FindWeapon(L.Primary)) { Names.Add(DescribeWeapon(L.Primary)); }
	if (FindWeapon(L.Secondary)) { Names.Add(DescribeWeapon(L.Secondary)); }
	return Names;
}

// ---- Market / armory ------------------------------------------------------------------------

int32 UZSProfileSubsystem::MarketDay()
{
	return static_cast<int32>(FDateTime::UtcNow().GetTicks() / ETimespan::TicksPerDay);
}

FTimespan UZSProfileSubsystem::TimeToMarketRefresh()
{
	const FDateTime Now = FDateTime::UtcNow();
	return (Now.GetDate() + FTimespan::FromDays(1.0)) - Now;
}

TArray<FZSMarketOffer> UZSProfileSubsystem::GetMarketOffers() const
{
	// Seeded by the UTC day: the stock is the same all day and changes at midnight UTC.
	FRandomStream Rng(MarketDay() * 7919 + 17);
	TArray<FZSMarketOffer> Offers;
	TArray<const ZSWeaponCatalog::FEntry*> Rotating;
	for (const ZSWeaponCatalog::FEntry& E : ZSWeaponCatalog::All())
	{
		if (E.bStarter)
		{
			FZSMarketOffer& O = Offers.AddDefaulted_GetRef();
			O.ItemId = E.ItemId;
			O.Price = O.FullPrice = E.Price;
			O.bStarter = true;
		}
		else
		{
			Rotating.Add(&E);
		}
	}
	for (int32 i = Rotating.Num() - 1; i > 0; --i)
	{
		Rotating.Swap(i, Rng.RandRange(0, i));
	}
	for (int32 i = 0; i < FMath::Min(DailyWeaponSlots, Rotating.Num()); ++i)
	{
		FZSMarketOffer& O = Offers.AddDefaulted_GetRef();
		O.ItemId = Rotating[i]->ItemId;
		O.Price = O.FullPrice = Rotating[i]->Price;
	}
	if (Offers.Num() > 0)
	{
		FZSMarketOffer& Deal = Offers[Rng.RandRange(0, Offers.Num() - 1)];
		Deal.bDeal = true;
		Deal.Price = FMath::RoundToInt(Deal.FullPrice * (1.f - DealDiscount) / 10.f) * 10;
	}
	return Offers;
}

bool UZSProfileSubsystem::BuyWeapon(FName ItemId, FText& OutMessage)
{
	const ZSWeaponCatalog::FEntry* E = ZSWeaponCatalog::Find(ItemId);
	int32 Price = -1;
	for (const FZSMarketOffer& O : GetMarketOffers())
	{
		if (O.ItemId == ItemId) { Price = O.Price; }
	}
	if (!Profile || !E || Price < 0)
	{
		OutMessage = NSLOCTEXT("ZS", "NotInStock", "Not in stock today.");
		return false;
	}
	if (GetLevel() < E->RequiredLevel)
	{
		OutMessage = FText::Format(NSLOCTEXT("ZS", "NeedLevel", "Requires level {0}."), FText::AsNumber(E->RequiredLevel));
		return false;
	}
	if (Profile->Cash < Price)
	{
		OutMessage = FText::Format(NSLOCTEXT("ZS", "NeedCash", "Not enough cash (${0} needed)."), FText::AsNumber(Price));
		return false;
	}
	if (Profile->Weapons.Num() >= MaxWeapons)
	{
		OutMessage = NSLOCTEXT("ZS", "InvFull", "Armory full - sell something first.");
		return false;
	}
	Profile->Cash -= Price;
	const FGuid Id = AddWeapon(ItemId);
	FZSLoadout& L = ActiveLoadoutRef();
	FGuid& Slot = E->bPrimary ? L.Primary : L.Secondary;
	if (!FindWeapon(Slot)) { Slot = Id; }
	Save();
	OutMessage = FText::Format(NSLOCTEXT("ZS", "Bought", "Bought {0}."), E->DisplayName);
	return true;
}

int32 UZSProfileSubsystem::GetSellPrice(const FGuid& WeaponId) const
{
	const FZSInventoryWeapon* W = FindWeapon(WeaponId);
	const ZSWeaponCatalog::FEntry* E = W ? ZSWeaponCatalog::Find(W->ItemId) : nullptr;
	if (!E)
	{
		return 0;
	}
	int32 Value = E->Price;
	for (const FName A : W->Attachments)
	{
		if (const ZSWeaponCatalog::FAttachment* Att = ZSWeaponCatalog::FindAttachment(A)) { Value += Att->Price; }
	}
	return FMath::RoundToInt(Value * SellFraction / 10.f) * 10;
}

bool UZSProfileSubsystem::SellWeapon(const FGuid& WeaponId, FText& OutMessage)
{
	if (!FindWeapon(WeaponId))
	{
		return false;
	}
	const int32 Price = GetSellPrice(WeaponId);
	OutMessage = FText::Format(NSLOCTEXT("ZS", "Sold", "Sold {0} for ${1}."), DescribeWeapon(WeaponId), FText::AsNumber(Price));
	Profile->Cash += Price;
	Profile->Weapons.RemoveAll([&WeaponId](const FZSInventoryWeapon& X) { return X.InstanceId == WeaponId; });
	CleanLoadouts();
	FillEmptySlots(ActiveLoadoutRef());
	Save();
	return true;
}

FText UZSProfileSubsystem::CanFitAttachment(const FGuid& WeaponId, FName AttachmentId) const
{
	const FZSInventoryWeapon* W = FindWeapon(WeaponId);
	const ZSWeaponCatalog::FEntry* E = W ? ZSWeaponCatalog::Find(W->ItemId) : nullptr;
	const ZSWeaponCatalog::FAttachment* A = ZSWeaponCatalog::FindAttachment(AttachmentId);
	if (!E || !A || !E->Attachments.Contains(AttachmentId))
	{
		return NSLOCTEXT("ZS", "AttIncompatible", "Doesn't fit this gun.");
	}
	if (W->Attachments.Contains(AttachmentId))
	{
		return NSLOCTEXT("ZS", "AttFitted", "Fitted.");
	}
	for (const FName Other : W->Attachments)
	{
		const ZSWeaponCatalog::FAttachment* O = ZSWeaponCatalog::FindAttachment(Other);
		if (O && O->Slot == A->Slot)
		{
			return FText::Format(NSLOCTEXT("ZS", "AttSlotTaken", "{0} slot taken by {1}."), ZSWeaponCatalog::SlotName(A->Slot), O->DisplayName);
		}
	}
	if (W->Attachments.Num() >= ZSWeaponCatalog::MaxAttachments)
	{
		return NSLOCTEXT("ZS", "AttMax", "This gun already has 5 attachments.");
	}
	if (GetLevel() < A->RequiredLevel)
	{
		return FText::Format(NSLOCTEXT("ZS", "NeedLevel", "Requires level {0}."), FText::AsNumber(A->RequiredLevel));
	}
	if (GetCash() < A->Price)
	{
		return FText::Format(NSLOCTEXT("ZS", "NeedCash", "Not enough cash (${0} needed)."), FText::AsNumber(A->Price));
	}
	return FText::GetEmpty();
}

bool UZSProfileSubsystem::BuyAttachment(const FGuid& WeaponId, FName AttachmentId, FText& OutMessage)
{
	OutMessage = CanFitAttachment(WeaponId, AttachmentId);
	if (!OutMessage.IsEmpty())
	{
		return false;
	}
	const ZSWeaponCatalog::FAttachment* A = ZSWeaponCatalog::FindAttachment(AttachmentId);
	Profile->Cash -= A->Price;
	FindWeaponMutable(WeaponId)->Attachments.Add(AttachmentId);
	Save();
	OutMessage = FText::Format(NSLOCTEXT("ZS", "Fitted2", "Fitted {0}."), A->DisplayName);
	return true;
}

bool UZSProfileSubsystem::RemoveAttachment(const FGuid& WeaponId, FName AttachmentId)
{
	FZSInventoryWeapon* W = FindWeaponMutable(WeaponId);
	if (W && W->Attachments.Remove(AttachmentId) > 0)
	{
		Save();
		return true;
	}
	return false;
}

// ---- Runs -----------------------------------------------------------------------------------

void UZSProfileSubsystem::FillEmptySlots(FZSLoadout& L)
{
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
	PickSlot(L.Primary, true);
	PickSlot(L.Secondary, false);
}

TArray<FZSInventoryWeapon> UZSProfileSubsystem::BeginRun()
{
	if (!Profile)
	{
		return {};
	}
	// Never deploy unarmed: with no weapons at all the quartermaster lends a starter sidearm (it becomes yours).
	if (Profile->Weapons.Num() == 0)
	{
		for (const ZSWeaponCatalog::FEntry& E : ZSWeaponCatalog::All())
		{
			if (E.bStarter && !E.bPrimary) { ActiveLoadoutRef().Secondary = AddWeapon(E.ItemId); break; }
		}
		UE_LOG(LogZS, Log, TEXT("No weapons owned: issued a free sidearm."));
	}

	// Fill empty / invalid slots of the active preset with an owned weapon of the right kind.
	FZSLoadout& L = ActiveLoadoutRef();
	FillEmptySlots(L);

	Profile->DeployedWeapons.Reset();
	if (L.Primary.IsValid()) { Profile->DeployedWeapons.Add(L.Primary); }
	if (L.Secondary.IsValid()) { Profile->DeployedWeapons.Add(L.Secondary); }
	Profile->bRunActive = true;
	Save();
	return GetDeployedWeapons();
}

bool UZSProfileSubsystem::IsRunActive() const
{
	return Profile && Profile->bRunActive;
}

TArray<FZSInventoryWeapon> UZSProfileSubsystem::GetDeployedWeapons() const
{
	TArray<FZSInventoryWeapon> Out;
	if (Profile)
	{
		for (const FGuid& Id : Profile->DeployedWeapons)
		{
			if (const FZSInventoryWeapon* W = FindWeapon(Id))
			{
				Out.Add(*W);
			}
		}
	}
	return Out;
}

TArray<FName> UZSProfileSubsystem::GetDeployedItemIds() const
{
	TArray<FName> Ids;
	for (const FZSInventoryWeapon& W : GetDeployedWeapons())
	{
		Ids.Add(W.ItemId);
	}
	return Ids;
}

TArray<FText> UZSProfileSubsystem::GetAtRiskNames() const
{
	TArray<FText> Names;
	if (Profile)
	{
		for (const FGuid& Id : Profile->DeployedWeapons)
		{
			Names.Add(DescribeWeapon(Id));
		}
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
		for (const ZSWeaponCatalog::FEntry& E : ZSWeaponCatalog::All())
		{
			if (E.RequiredLevel == L) { R.Unlocks.Add(FText::Format(NSLOCTEXT("ZS", "UnlockWeapon", "Market: {0}"), E.DisplayName)); }
		}
		for (const ZSWeaponCatalog::FAttachment& A : ZSWeaponCatalog::Attachments())
		{
			if (A.RequiredLevel == L) { R.Unlocks.Add(FText::Format(NSLOCTEXT("ZS", "UnlockAtt", "Armory: {0}"), A.DisplayName)); }
		}
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
	EnsureLoadouts();
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
