#include "Bunker/ZSBunkerMenus.h"
#include "Player/ZSPlayerController.h"
#include "Profile/ZSProfileSubsystem.h"
#include "Core/ZSLevelCatalog.h"
#include "Weapons/ZSWeaponCatalog.h"
#include "Weapons/ZSWeaponBase.h"
#include "UI/ZSHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Bunker/ZSStation.h"
#include "Core/ZSCharacterCatalog.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Missions/ZSMissionSubsystem.h"
#include "Zombieshooter.h"

namespace ZSBunkerMenus
{
	using FWeakPC = TWeakObjectPtr<AZSPlayerController>;

	static UZSProfileSubsystem* ProfileOf(const FWeakPC& PC)
	{
		return PC.IsValid() ? UZSProfileSubsystem::Get(PC.Get()) : nullptr;
	}

	static void Say(const FWeakPC& PC, const FText& Message)
	{
		if (PC.IsValid())
		{
			AZSHUD::Toast(PC->GetPawn(), Message);
		}
	}

	static FText Money(int64 Amount)
	{
		return FText::Format(NSLOCTEXT("ZS", "Money", "${0}"), FText::AsNumber(Amount));
	}

	static FText CashLine(const UZSProfileSubsystem* Profile)
	{
		if (Profile->GetPrestige() > 0)
		{
			return FText::Format(NSLOCTEXT("ZS", "CashLineP", "Cash {0}   Level {1}   Prestige {2} ({3})"), Money(Profile->GetCash()),
				FText::AsNumber(Profile->GetLevel()), FText::AsNumber(Profile->GetPrestige()), UZSProfileSubsystem::PrestigeTier(Profile->GetPrestige()));
		}
		return FText::Format(NSLOCTEXT("ZS", "CashLine", "Cash {0}   Level {1}"), Money(Profile->GetCash()), FText::AsNumber(Profile->GetLevel()));
	}

	static FZSMenuItem BackItem(const FWeakPC& PC)
	{
		return FZSMenuItem(NSLOCTEXT("ZS", "Back", "BACK"), [PC]() { if (PC.IsValid()) { PC->PopMenu(); } });
	}

	/** Stat card for a weapon class with the given attachments applied. */
	static FText StatCard(FName ItemId, const TArray<FName>& Attachments)
	{
		const ZSWeaponCatalog::FEntry* E = ZSWeaponCatalog::Find(ItemId);
		const AZSWeaponBase* CDO = (E && E->Class) ? E->Class->GetDefaultObject<AZSWeaponBase>() : nullptr;
		if (!CDO)
		{
			return FText::GetEmpty();
		}
		FZSWeaponStats S = CDO->Stats;
		for (const FName A : Attachments)
		{
			if (const ZSWeaponCatalog::FAttachment* Att = ZSWeaponCatalog::FindAttachment(A); Att && Att->Apply) { Att->Apply(S); }
		}
		float Recoil = 0.f;
		for (const FVector2D& R : S.RecoilPattern) { Recoil += R.Size(); }
		Recoil = S.RecoilPattern.Num() > 0 ? Recoil / S.RecoilPattern.Num() : 0.f;
		FString Card = FString::Printf(TEXT("Damage %.0f   RPM %.0f   Mag %d\nRange %.0f m   Reload %.1f s   ADS %.2f s\nRecoil %.2f   Hip spread %.1f"),
			S.Damage, S.FireRateRPM, S.MagSize, S.FalloffStart / 100.f, S.TacticalReloadTime, S.ADSTime, Recoil, S.HipSpread);
		if (Attachments.Num() > 0)
		{
			Card += TEXT("\nAttachments:");
			for (const FName A : Attachments) { Card += TEXT(" ") + ZSWeaponCatalog::FindAttachment(A)->DisplayName.ToString() + TEXT(","); }
			Card.RemoveFromEnd(TEXT(","));
		}
		return FText::FromString(Card);
	}

	// ---- Map table ------------------------------------------------------------------------------

	void Open(AZSPlayerController* PC, EZSStationType Type)
	{
		switch (Type)
		{
		case EZSStationType::MapTable:  OpenMapTable(PC); break;
		case EZSStationType::Armory:    OpenArmory(PC); break;
		case EZSStationType::Market:    OpenMarket(PC); break;
		case EZSStationType::Character: OpenCharacter(PC); break;
		case EZSStationType::Missions:  OpenMissions(PC); break;
		}
	}

	void Deploy(AZSPlayerController* PC, FName LevelId)
	{
		const ZSLevelCatalog::FEntry* L = ZSLevelCatalog::Find(LevelId);
		if (!PC || !L || L->MapURL.IsEmpty())
		{
			return;
		}
		FString Map, Options;
		if (!L->MapURL.Split(TEXT("?"), &Map, &Options))
		{
			Map = L->MapURL;
		}
		PC->CloseAllMenus();
		UE_LOG(LogZS, Log, TEXT("Deploying to %s"), *L->MapURL);
		UGameplayStatics::OpenLevel(PC, FName(*Map), true, Options);
	}

	static FText LoadoutSummary(const UZSProfileSubsystem* Profile, int32 Index = -1)
	{
		FString S;
		for (const FText& N : Profile->GetLoadoutNames(Index))
		{
			S += (S.IsEmpty() ? TEXT("") : TEXT(", ")) + N.ToString();
		}
		return FText::FromString(S.IsEmpty() ? TEXT("(empty - buy a weapon at the market)") : S);
	}

	void OpenMapTable(AZSPlayerController* PC)
	{
		if (!PC) { return; }
		FWeakPC WeakPC(PC);
		FZSMenuPage Page;
		Page.Title = NSLOCTEXT("ZS", "MapTitle", "MAP TABLE");
		Page.Refresh = [WeakPC](FZSMenuPage& P)
		{
			const UZSProfileSubsystem* Profile = ProfileOf(WeakPC);
			if (!Profile) { return; }
			P.Subtitle = FText::Format(NSLOCTEXT("ZS", "MapSub", "Choose where to deploy.   Your level: {0}"), FText::AsNumber(Profile->GetLevel()));
			P.Items.Reset();
			for (const ZSLevelCatalog::FEntry& L : ZSLevelCatalog::All())
			{
				const bool bUnlocked = Profile->GetUnlockLevel() >= L.RequiredLevel;
				const bool bBuilt = !L.MapURL.IsEmpty();
				FText Value = !bUnlocked ? FText::Format(NSLOCTEXT("ZS", "LockedLvl", "LOCKED - LEVEL {0}"), FText::AsNumber(L.RequiredLevel))
					: (bBuilt ? NSLOCTEXT("ZS", "Available", "AVAILABLE") : NSLOCTEXT("ZS", "ComingSoon", "COMING SOON"));
				const FName Id = L.Id;
				P.Items.Emplace(L.DisplayName, [WeakPC, Id]()
				{
					const UZSProfileSubsystem* Prof = ProfileOf(WeakPC);
					const ZSLevelCatalog::FEntry* Lvl = ZSLevelCatalog::Find(Id);
					if (!Prof || !Lvl) { return; }
					// Deploy confirmation: what's at stake.
					FZSMenuPage Confirm;
					Confirm.Title = FText::Format(NSLOCTEXT("ZS", "DeployTo", "DEPLOY: {0}"), Lvl->DisplayName);
					Confirm.Refresh = [WeakPC, Id](FZSMenuPage& C)
					{
						const UZSProfileSubsystem* Pr = ProfileOf(WeakPC);
						if (!Pr) { return; }
						C.Subtitle = FText::Format(NSLOCTEXT("ZS", "AtRiskLine", "AT RISK: {0}"), LoadoutSummary(Pr));
						const FText Detail = NSLOCTEXT("ZS", "DeployDetail", "If you die or abandon the run you lose the weapons you take (with their attachments) and everything you earn during the run. Kill the round-20 boss to bring it all home, plus a win bonus.");
						C.Items.Reset();
						const bool bArmed = Pr->GetWeapons().Num() > 0;
						C.Items.Emplace(NSLOCTEXT("ZS", "DeployNow", "DEPLOY NOW"), [WeakPC, Id]() { if (WeakPC.IsValid()) { Deploy(WeakPC.Get(), Id); } },
							bArmed ? FText::GetEmpty() : NSLOCTEXT("ZS", "LoanerTag", "LOANER P9"),
							bArmed ? Detail : NSLOCTEXT("ZS", "NoWeaponsDetail", "You own no weapons, so the quartermaster will hand you a P9 sidearm. Buy better guns at the MARKET."));
						C.Items.Emplace(NSLOCTEXT("ZS", "ChangeLoadout", "CHANGE LOADOUT (ARMORY)"), [WeakPC]() { if (WeakPC.IsValid()) { OpenArmory(WeakPC.Get()); } });
						C.Items.Add(BackItem(WeakPC));
					};
					WeakPC->PushMenu(Confirm);
				}, Value, L.Description, bUnlocked && bBuilt);
			}
			P.Items.Add(BackItem(WeakPC));
		};
		PC->PushMenu(Page);
	}

	// ---- Armory ---------------------------------------------------------------------------------

	static void OpenWeaponPage(const FWeakPC& WeakPC, FGuid WeaponId);

	/** Pick which owned weapon goes in a preset's primary / sidearm slot. */
	static void OpenSlotPicker(const FWeakPC& WeakPC, int32 Loadout, bool bPrimary)
	{
		FZSMenuPage Page;
		Page.Title = FText::Format(bPrimary ? NSLOCTEXT("ZS", "PickPrimary", "LOADOUT {0}: PRIMARY") : NSLOCTEXT("ZS", "PickSidearm", "LOADOUT {0}: SIDEARM"), FText::AsNumber(Loadout + 1));
		Page.Refresh = [WeakPC, Loadout, bPrimary](FZSMenuPage& P)
		{
			UZSProfileSubsystem* Profile = ProfileOf(WeakPC);
			if (!Profile) { return; }
			const FZSLoadout L = Profile->GetLoadout(Loadout);
			const FGuid Current = bPrimary ? L.Primary : L.Secondary;
			P.Items.Reset();
			for (const FZSInventoryWeapon& W : Profile->GetWeapons())
			{
				const ZSWeaponCatalog::FEntry* E = ZSWeaponCatalog::Find(W.ItemId);
				if (!E || E->bPrimary != bPrimary) { continue; }
				const FGuid Id = W.InstanceId;
				P.Items.Emplace(Profile->DescribeWeapon(Id), [WeakPC, Loadout, Id]()
				{
					if (UZSProfileSubsystem* Pr = ProfileOf(WeakPC)) { Pr->AssignToLoadout(Loadout, Id); }
					if (WeakPC.IsValid()) { WeakPC->PopMenu(); }
				}, Id == Current ? NSLOCTEXT("ZS", "Equipped", "EQUIPPED") : FText::GetEmpty(), StatCard(W.ItemId, W.Attachments));
			}
			P.Items.Emplace(NSLOCTEXT("ZS", "EmptyTheSlot", "(EMPTY SLOT)"), [WeakPC, Loadout, bPrimary]()
			{
				if (UZSProfileSubsystem* Pr = ProfileOf(WeakPC)) { Pr->ClearLoadoutSlot(Loadout, bPrimary); }
				if (WeakPC.IsValid()) { WeakPC->PopMenu(); }
			});
			P.Items.Add(BackItem(WeakPC));
		};
		WeakPC->PushMenu(Page);
	}

	static void OpenLoadoutPage(const FWeakPC& WeakPC, int32 Loadout)
	{
		FZSMenuPage Page;
		Page.Title = FText::Format(NSLOCTEXT("ZS", "LoadoutN", "LOADOUT {0}"), FText::AsNumber(Loadout + 1));
		Page.Refresh = [WeakPC, Loadout](FZSMenuPage& P)
		{
			UZSProfileSubsystem* Profile = ProfileOf(WeakPC);
			if (!Profile) { return; }
			const FZSLoadout L = Profile->GetLoadout(Loadout);
			const bool bActive = Profile->GetActiveLoadoutIndex() == Loadout;
			P.Subtitle = bActive ? NSLOCTEXT("ZS", "LoadoutActive", "This is the loadout you deploy with.") : NSLOCTEXT("ZS", "LoadoutInactive", "Not active.");
			P.Items.Reset();
			P.Items.Emplace(NSLOCTEXT("ZS", "SetActive", "DEPLOY WITH THIS LOADOUT"), [WeakPC, Loadout]()
			{
				if (UZSProfileSubsystem* Pr = ProfileOf(WeakPC)) { Pr->SetActiveLoadout(Loadout); }
			}, bActive ? NSLOCTEXT("ZS", "Active", "ACTIVE") : FText::GetEmpty(), FText::GetEmpty(), !bActive);
			const FZSInventoryWeapon* Pri = Profile->FindWeapon(L.Primary);
			const FZSInventoryWeapon* Sec = Profile->FindWeapon(L.Secondary);
			P.Items.Emplace(NSLOCTEXT("ZS", "PrimarySlot", "PRIMARY"), [WeakPC, Loadout]() { OpenSlotPicker(WeakPC, Loadout, true); },
				Profile->DescribeWeapon(L.Primary), Pri ? StatCard(Pri->ItemId, Pri->Attachments) : FText::GetEmpty());
			P.Items.Emplace(NSLOCTEXT("ZS", "SidearmSlot", "SIDEARM"), [WeakPC, Loadout]() { OpenSlotPicker(WeakPC, Loadout, false); },
				Profile->DescribeWeapon(L.Secondary), Sec ? StatCard(Sec->ItemId, Sec->Attachments) : FText::GetEmpty());
			P.Items.Add(BackItem(WeakPC));
		};
		WeakPC->PushMenu(Page);
	}

	static FText WeaponLevelLine(const UZSProfileSubsystem* Profile, FName ItemId)
	{
		int32 XP = 0, Needed = 0;
		Profile->GetWeaponLevelProgress(ItemId, XP, Needed);
		const int32 Level = Profile->GetWeaponLevel(ItemId);
		return Needed > 0
			? FText::Format(NSLOCTEXT("ZS", "WLvlLine", "LV {0}  ({1}/{2} XP)"), FText::AsNumber(Level), FText::AsNumber(XP), FText::AsNumber(Needed))
			: FText::Format(NSLOCTEXT("ZS", "WLvlMax", "LV {0}  (MAX)"), FText::AsNumber(Level));
	}

	static void OpenCamoPage(const FWeakPC& WeakPC, FName ItemId)
	{
		FZSMenuPage Page;
		Page.Refresh = [WeakPC, ItemId](FZSMenuPage& P)
		{
			UZSProfileSubsystem* Profile = ProfileOf(WeakPC);
			if (!Profile) { return; }
			P.Title = FText::Format(NSLOCTEXT("ZS", "CamoTitle", "{0} CAMOS"), FText::FromString(ZSWeaponCatalog::DisplayName(ItemId).ToString().ToUpper()));
			P.Subtitle = FText::Format(NSLOCTEXT("ZS", "CamoSub", "{0}   Camos unlock as the gun levels up (kills with it, in any run)"), WeaponLevelLine(Profile, ItemId));
			P.Items.Reset();
			const int32 Level = Profile->GetWeaponLevel(ItemId);
			const FName Current = Profile->GetWeaponCamo(ItemId);
			for (const ZSWeaponCatalog::FCamo& C : ZSWeaponCatalog::Camos())
			{
				const bool bOn = Current == C.Id || (Current.IsNone() && C.Id == TEXT("NONE"));
				const bool bLocked = Level < C.RequiredLevel;
				const FName Id = C.Id;
				P.Items.Emplace(C.DisplayName, [WeakPC, ItemId, Id]()
				{
					FText Msg;
					if (UZSProfileSubsystem* Pr = ProfileOf(WeakPC)) { Pr->SetWeaponCamo(ItemId, Id, Msg); Say(WeakPC, Msg); }
				}, bOn ? NSLOCTEXT("ZS", "CamoOn", "EQUIPPED") : (bLocked ? FText::Format(NSLOCTEXT("ZS", "CamoLvl", "WEAPON LV {0}"), FText::AsNumber(C.RequiredLevel)) : FText::GetEmpty()),
					FText::GetEmpty(), !bLocked);
			}
			P.Items.Add(BackItem(WeakPC));
		};
		WeakPC->PushMenu(Page);
	}

	static void OpenLevelsPage(const FWeakPC& WeakPC)
	{
		FZSMenuPage Page;
		Page.Title = NSLOCTEXT("ZS", "LevelsTitle", "WEAPON LEVELS");
		Page.Refresh = [WeakPC](FZSMenuPage& P)
		{
			UZSProfileSubsystem* Profile = ProfileOf(WeakPC);
			if (!Profile) { return; }
			P.Subtitle = NSLOCTEXT("ZS", "LevelsSub", "Every gun starts at level 1. Kills with it (your own, wall buys, the box) level it up and unlock its attachments and camos.");
			P.Items.Reset();
			for (const ZSWeaponCatalog::FEntry& E : ZSWeaponCatalog::All())
			{
				const FName Id = E.ItemId;
				FString Next;
				const int32 Level = Profile->GetWeaponLevel(Id);
				for (const FName AttId : E.Attachments)
				{
					const ZSWeaponCatalog::FAttachment* A = ZSWeaponCatalog::FindAttachment(AttId);
					if (A && A->RequiredLevel > Level) { Next += FString::Printf(TEXT("LV %d  %s\n"), A->RequiredLevel, *A->DisplayName.ToString()); }
				}
				for (const ZSWeaponCatalog::FCamo& C : ZSWeaponCatalog::Camos())
				{
					if (C.RequiredLevel > Level) { Next += FString::Printf(TEXT("LV %d  %s camo\n"), C.RequiredLevel, *C.DisplayName.ToString()); }
				}
				P.Items.Emplace(E.DisplayName, [WeakPC, Id]() { OpenCamoPage(WeakPC, Id); }, WeaponLevelLine(Profile, Id),
					FText::Format(NSLOCTEXT("ZS", "LevelsDetail", "Coming up:\n{0}\nSelect to pick a camo."), FText::FromString(Next.IsEmpty() ? TEXT("Everything unlocked.") : Next)));
			}
			P.Items.Add(BackItem(WeakPC));
		};
		WeakPC->PushMenu(Page);
	}

	static void OpenWeaponPage(const FWeakPC& WeakPC, FGuid WeaponId)
	{
		FZSMenuPage Page;
		Page.Refresh = [WeakPC, WeaponId](FZSMenuPage& P)
		{
			UZSProfileSubsystem* Profile = ProfileOf(WeakPC);
			const FZSInventoryWeapon* W = Profile ? Profile->FindWeapon(WeaponId) : nullptr;
			const ZSWeaponCatalog::FEntry* E = W ? ZSWeaponCatalog::Find(W->ItemId) : nullptr;
			P.Items.Reset();
			if (!W || !E)
			{
				P.Title = NSLOCTEXT("ZS", "WeaponGone", "WEAPON");
				P.Items.Add(BackItem(WeakPC));
				return;
			}
			P.Title = FText::FromString(Profile->DescribeWeapon(WeaponId).ToString().ToUpper());
			P.Subtitle = FText::Format(NSLOCTEXT("ZS", "WeaponSub2", "{0}   {1}   Attachments {2}/{3}   (removing an attachment scraps it)"),
				CashLine(Profile), WeaponLevelLine(Profile, W->ItemId), FText::AsNumber(W->Attachments.Num()), FText::AsNumber(ZSWeaponCatalog::MaxAttachments));
			for (const FName AttId : E->Attachments)
			{
				const ZSWeaponCatalog::FAttachment* A = ZSWeaponCatalog::FindAttachment(AttId);
				if (!A) { continue; }
				const bool bFitted = W->Attachments.Contains(AttId);
				const FText Why = Profile->CanFitAttachment(WeaponId, AttId);
				const FText Label = FText::Format(NSLOCTEXT("ZS", "AttLabel", "{0}: {1}"), ZSWeaponCatalog::SlotName(A->Slot), A->DisplayName);
				FText Value = bFitted ? NSLOCTEXT("ZS", "FittedTag", "FITTED - REMOVE")
					: (Profile->GetWeaponLevel(W->ItemId) < A->RequiredLevel ? FText::Format(NSLOCTEXT("ZS", "WLvlTag", "WEAPON LV {0}"), FText::AsNumber(A->RequiredLevel)) : Money(A->Price));
				FText Detail = Why.IsEmpty() || bFitted ? A->Description
					: FText::Format(NSLOCTEXT("ZS", "AttWhy", "{0}\n\n{1}"), A->Description, Why);
				P.Items.Emplace(Label, [WeakPC, WeaponId, AttId, bFitted]()
				{
					UZSProfileSubsystem* Pr = ProfileOf(WeakPC);
					if (!Pr) { return; }
					if (bFitted)
					{
						Pr->RemoveAttachment(WeaponId, AttId);
						Say(WeakPC, NSLOCTEXT("ZS", "AttRemoved", "Attachment removed."));
					}
					else
					{
						FText Msg;
						Pr->BuyAttachment(WeaponId, AttId, Msg);
						Say(WeakPC, Msg);
					}
				}, Value, Detail, bFitted || Why.IsEmpty());
			}
			{
				const FName ItemId = W->ItemId;
				const ZSWeaponCatalog::FCamo* Camo = ZSWeaponCatalog::FindCamo(Profile->GetWeaponCamo(ItemId));
				P.Items.Emplace(NSLOCTEXT("ZS", "CamoItem", "CAMO"), [WeakPC, ItemId]() { OpenCamoPage(WeakPC, ItemId); },
					Camo ? Camo->DisplayName : NSLOCTEXT("ZS", "CamoFactory", "Factory"), NSLOCTEXT("ZS", "CamoItemDetail", "Pattern for every gun of this type."));
			}
			const int32 SellPrice = Profile->GetSellPrice(WeaponId);
			P.Items.Emplace(NSLOCTEXT("ZS", "SellWeapon", "SELL"), [WeakPC, WeaponId]()
			{
				FZSMenuPage Confirm;
				Confirm.Title = NSLOCTEXT("ZS", "SellConfirm", "SELL THIS WEAPON?");
				Confirm.Items.Emplace(NSLOCTEXT("ZS", "SellYes", "YES, SELL IT"), [WeakPC, WeaponId]()
				{
					FText Msg;
					if (UZSProfileSubsystem* Pr = ProfileOf(WeakPC); Pr && Pr->SellWeapon(WeaponId, Msg)) { Say(WeakPC, Msg); }
					if (WeakPC.IsValid()) { WeakPC->PopMenu(); WeakPC->PopMenu(); }
				});
				Confirm.Items.Add(BackItem(WeakPC));
				WeakPC->PushMenu(Confirm);
			}, Money(SellPrice), NSLOCTEXT("ZS", "SellDetail", "Sells the gun and its attachments for 40% of what they cost."));
			P.Items.Add(BackItem(WeakPC));
			const FText Card = StatCard(W->ItemId, W->Attachments);
			for (FZSMenuItem& I : P.Items)
			{
				I.Detail = I.Detail.IsEmpty() ? Card : FText::Format(NSLOCTEXT("ZS", "DetailWithCard", "{0}\n\n{1}"), I.Detail, Card);
			}
		};
		WeakPC->PushMenu(Page);
	}

	void OpenArmory(AZSPlayerController* PC)
	{
		if (!PC) { return; }
		FWeakPC WeakPC(PC);
		FZSMenuPage Page;
		Page.Title = NSLOCTEXT("ZS", "ArmoryTitle", "ARMORY");
		Page.Refresh = [WeakPC](FZSMenuPage& P)
		{
			UZSProfileSubsystem* Profile = ProfileOf(WeakPC);
			if (!Profile) { return; }
			P.Subtitle = FText::Format(NSLOCTEXT("ZS", "ArmorySub", "{0}   Weapons {1}/{2}"), CashLine(Profile),
				FText::AsNumber(Profile->GetWeapons().Num()), FText::AsNumber(UZSProfileSubsystem::MaxWeapons));
			P.Items.Reset();
			for (int32 i = 0; i < UZSProfileSubsystem::NumLoadouts; ++i)
			{
				const bool bActive = Profile->GetActiveLoadoutIndex() == i;
				P.Items.Emplace(FText::Format(NSLOCTEXT("ZS", "LoadoutN", "LOADOUT {0}"), FText::AsNumber(i + 1)),
					[WeakPC, i]() { OpenLoadoutPage(WeakPC, i); },
					bActive ? NSLOCTEXT("ZS", "Active", "ACTIVE") : FText::GetEmpty(), LoadoutSummary(Profile, i));
			}
			P.Items.Emplace(NSLOCTEXT("ZS", "WeaponLevels", "WEAPON LEVELS & CAMOS"), [WeakPC]() { OpenLevelsPage(WeakPC); },
				FText::GetEmpty(), NSLOCTEXT("ZS", "WeaponLevelsDetail", "Every gun type's level, what it unlocks next, and its camo."));
			for (const FZSInventoryWeapon& W : Profile->GetWeapons())
			{
				const FGuid Id = W.InstanceId;
				P.Items.Emplace(Profile->DescribeWeapon(Id), [WeakPC, Id]() { OpenWeaponPage(WeakPC, Id); },
					FText::Format(NSLOCTEXT("ZS", "ModifyLv", "{0}  CUSTOMISE"), WeaponLevelLine(Profile, W.ItemId)), StatCard(W.ItemId, W.Attachments));
			}
			P.Items.Add(BackItem(WeakPC));
		};
		PC->PushMenu(Page);
	}

	// ---- Market ---------------------------------------------------------------------------------

	void OpenMarket(AZSPlayerController* PC)
	{
		if (!PC) { return; }
		FWeakPC WeakPC(PC);
		FZSMenuPage Page;
		Page.Title = NSLOCTEXT("ZS", "MarketTitle", "MARKET");
		Page.Refresh = [WeakPC](FZSMenuPage& P)
		{
			UZSProfileSubsystem* Profile = ProfileOf(WeakPC);
			if (!Profile) { return; }
			const FTimespan Left = UZSProfileSubsystem::TimeToMarketRefresh();
			P.Subtitle = FText::Format(NSLOCTEXT("ZS", "MarketSub", "{0}   New stock in {1}h {2}m"), CashLine(Profile),
				FText::AsNumber(Left.GetHours()), FText::AsNumber(Left.GetMinutes()));
			P.Items.Reset();
			for (const FZSMarketOffer& O : Profile->GetMarketOffers())
			{
				const ZSWeaponCatalog::FEntry* E = ZSWeaponCatalog::Find(O.ItemId);
				if (!E) { continue; }
				const bool bLocked = Profile->GetUnlockLevel() < E->RequiredLevel;
				FText Value = bLocked ? FText::Format(NSLOCTEXT("ZS", "LvlTag", "LEVEL {0}"), FText::AsNumber(E->RequiredLevel))
					: (O.bDeal ? FText::Format(NSLOCTEXT("ZS", "DealTag", "DEAL {0} (was {1})"), Money(O.Price), Money(O.FullPrice)) : Money(O.Price));
				FText Label = FText::Format(NSLOCTEXT("ZS", "OfferLabel", "{0}{1}"), E->DisplayName,
					O.bStarter ? NSLOCTEXT("ZS", "StarterTag", "  (always stocked)") : FText::GetEmpty());
				FText Detail = FText::Format(NSLOCTEXT("ZS", "OfferDetail", "{0}\n\n{1}\n\nSlot: {2}"), E->Description, StatCard(E->ItemId, {}),
					E->bPrimary ? NSLOCTEXT("ZS", "Primary", "Primary") : NSLOCTEXT("ZS", "Sidearm", "Sidearm"));
				const FName Id = O.ItemId;
				P.Items.Emplace(Label, [WeakPC, Id]()
				{
					FText Msg;
					if (UZSProfileSubsystem* Pr = ProfileOf(WeakPC)) { Pr->BuyWeapon(Id, Msg); }
					Say(WeakPC, Msg);
				}, Value, Detail, !bLocked && Profile->GetCash() >= O.Price);
			}
			P.Items.Add(BackItem(WeakPC));
		};
		PC->PushMenu(Page);
	}

	// ---- Placeholders (filled in by later milestones) --------------------------------------------

	static void PlaceholderPage(AZSPlayerController* PC, const FText& Title, const FText& Detail)
	{
		FZSMenuPage Page;
		Page.Title = Title;
		FWeakPC WeakPC(PC);
		FZSMenuItem Back = BackItem(WeakPC);
		Back.Detail = Detail;
		Page.Items.Add(Back);
		PC->PushMenu(Page);
	}

	void OpenCharacter(AZSPlayerController* PC)
	{
		if (!PC) { return; }
		FWeakPC WeakPC(PC);

		// Look through the locker's preview camera while the menu is open.
		AZSStation* Locker = nullptr;
		for (TActorIterator<AZSStation> It(PC->GetWorld()); It; ++It)
		{
			if (It->Type == EZSStationType::Character) { Locker = *It; break; }
		}

		FZSMenuPage Page;
		Page.Title = NSLOCTEXT("ZS", "LockerTitle", "LOCKER");
		Page.bHideDetail = true;
		if (Locker)
		{
			Locker->GetPreviewCamera()->SetActive(true);
			PC->SetViewTargetWithBlend(Locker, 0.4f, VTBlend_Cubic);
			TWeakObjectPtr<AZSStation> WeakLocker(Locker);
			Page.OnClosed = [WeakPC, WeakLocker]()
			{
				if (WeakPC.IsValid() && WeakPC->GetPawn()) { WeakPC->SetViewTargetWithBlend(WeakPC->GetPawn(), 0.3f, VTBlend_Cubic); }
				if (WeakLocker.IsValid()) { WeakLocker->GetPreviewCamera()->SetActive(false); }
			};
		}

		// Cycle to the next unlocked entry.
		auto Next = [](int32 Current, int32 Count, TFunctionRef<bool(int32)> Unlocked)
		{
			for (int32 Step = 1; Step <= Count; ++Step)
			{
				const int32 I = (Current + Step) % Count;
				if (Unlocked(I)) { return I; }
			}
			return Current;
		};

		Page.Refresh = [WeakPC, Next](FZSMenuPage& P)
		{
			UZSProfileSubsystem* Profile = ProfileOf(WeakPC);
			if (!Profile) { return; }
			const FZSCharacterLook Look = Profile->GetLook();
			const int32 Level = Profile->GetUnlockLevel();
			const auto& Bodies = ZSCharacterCatalog::Bodies();
			const auto& Arms = ZSCharacterCatalog::Arms();
			const auto& Tints = ZSCharacterCatalog::Tints();
			int32 BodiesUnlocked = 0, ArmsUnlocked = 0;
			for (const auto& B : Bodies) { BodiesUnlocked += Level >= B.RequiredLevel; }
			for (const auto& A : Arms) { ArmsUnlocked += Level >= A.RequiredLevel; }
			P.Subtitle = FText::Format(NSLOCTEXT("ZS", "LockerSub", "Select to change. Outfits are never lost.   Bodies {0}/{1}   Gloves {2}/{3} (more unlock as you level)"),
				FText::AsNumber(BodiesUnlocked), FText::AsNumber(Bodies.Num()), FText::AsNumber(ArmsUnlocked), FText::AsNumber(Arms.Num()));
			P.Items.Reset();

			const int32 ArmsShown = ZSCharacterCatalog::ResolveArms(Look);
			const int32 TintShown = ZSCharacterCatalog::ResolveTint(Look);
			P.Items.Emplace(NSLOCTEXT("ZS", "LkBody", "SURVIVOR"), [WeakPC, Next]()
			{
				UZSProfileSubsystem* Pr = ProfileOf(WeakPC);
				if (!Pr) { return; }
				FZSCharacterLook L = Pr->GetLook();
				const int32 Lvl = Pr->GetUnlockLevel();
				L.Body = Next(L.Body, ZSCharacterCatalog::Bodies().Num(), [Lvl](int32 I) { return Lvl >= ZSCharacterCatalog::Bodies()[I].RequiredLevel; });
				Pr->SetLook(L);
			}, Bodies.IsValidIndex(Look.Body) ? Bodies[Look.Body].DisplayName : FText::GetEmpty());
			P.Items.Emplace(NSLOCTEXT("ZS", "LkMatch", "MATCH GLOVES TO OUTFIT"), [WeakPC]()
			{
				UZSProfileSubsystem* Pr = ProfileOf(WeakPC);
				if (!Pr) { return; }
				FZSCharacterLook L = Pr->GetLook();
				if (L.bMatchArms)
				{
					// Start custom from what the outfit was showing.
					L.Arms = ZSCharacterCatalog::ResolveArms(L);
					L.Tint = ZSCharacterCatalog::ResolveTint(L);
				}
				L.bMatchArms = !L.bMatchArms;
				Pr->SetLook(L);
			}, Look.bMatchArms ? NSLOCTEXT("ZS", "On", "ON") : NSLOCTEXT("ZS", "Off", "OFF"));
			P.Items.Emplace(NSLOCTEXT("ZS", "LkArms", "GLOVES"), [WeakPC, Next]()
			{
				UZSProfileSubsystem* Pr = ProfileOf(WeakPC);
				if (!Pr) { return; }
				FZSCharacterLook L = Pr->GetLook();
				const int32 Lvl = Pr->GetUnlockLevel();
				L.Arms = Next(L.Arms, ZSCharacterCatalog::Arms().Num(), [Lvl](int32 I) { return Lvl >= ZSCharacterCatalog::Arms()[I].RequiredLevel; });
				Pr->SetLook(L);
			}, Arms[ArmsShown].DisplayName, FText::GetEmpty(), !Look.bMatchArms);
			P.Items.Emplace(NSLOCTEXT("ZS", "LkTint", "GLOVE TINT"), [WeakPC, Next]()
			{
				UZSProfileSubsystem* Pr = ProfileOf(WeakPC);
				if (!Pr) { return; }
				FZSCharacterLook L = Pr->GetLook();
				const int32 Rank = Pr->GetPrestige();
				L.Tint = Next(L.Tint, ZSCharacterCatalog::Tints().Num(), [Rank](int32 I) { return Rank >= ZSCharacterCatalog::Tints()[I].RequiredPrestige; });
				Pr->SetLook(L);
			}, Tints[TintShown].DisplayName, FText::GetEmpty(), !Look.bMatchArms);
			P.Items.Add(BackItem(WeakPC));
		};
		PC->PushMenu(Page);
	}

	void OpenMissions(AZSPlayerController* PC)
	{
		if (!PC) { return; }
		FWeakPC WeakPC(PC);
		FZSMenuPage Page;
		Page.Title = NSLOCTEXT("ZS", "MissionsTitle", "MISSION BOARD");
		Page.Refresh = [WeakPC](FZSMenuPage& P)
		{
			const UZSMissionSubsystem* Missions = WeakPC.IsValid() ? UZSMissionSubsystem::Get(WeakPC.Get()) : nullptr;
			if (!Missions) { return; }
			const FTimespan D = UZSMissionSubsystem::TimeToDailyReset();
			const FTimespan W = UZSMissionSubsystem::TimeToWeeklyReset();
			P.Subtitle = FText::Format(NSLOCTEXT("ZS", "MissionsSub", "Dailies reset in {0}h {1}m   Weeklies in {2}d {3}h   Story chapter {4}/{5}"),
				FText::AsNumber(D.GetHours()), FText::AsNumber(D.GetMinutes()), FText::AsNumber(W.GetDays()), FText::AsNumber(W.GetHours()),
				FText::AsNumber(FMath::Min(Missions->GetStoryChapter() + 1, Missions->GetStoryChapterCount())), FText::AsNumber(Missions->GetStoryChapterCount()));
			P.Items.Reset();
			if (const UZSProfileSubsystem* Pr = ProfileOf(WeakPC))
			{
				const int32 Rank = Pr->GetPrestige();
				if (Rank >= UZSProfileSubsystem::MaxPrestige)
				{
					P.Items.Emplace(NSLOCTEXT("ZS", "PrMasterLabel", "PRESTIGE   Master"), []() {}, NSLOCTEXT("ZS", "PrMaxTag", "MAXED"),
						NSLOCTEXT("ZS", "PrMasterDetail", "You have reached the final prestige rank. Every bonus and cosmetic is yours."));
				}
				else
				{
					FString Rewards;
					for (const FText& T : UZSProfileSubsystem::PrestigeRewards(Rank + 1)) { Rewards += TEXT("- ") + T.ToString() + TEXT("\n"); }
					const FText Detail = FText::Format(NSLOCTEXT("ZS", "PrDetail", "Prestige to rank {0} ({1}) once you reach level {2}.\n\nYour level goes back to 1 and climbs again at {3}% of the usual XP. You KEEP every weapon, attachment, outfit, unlock and all your cash.\n\nRewards:\n{4}"),
						FText::AsNumber(Rank + 1), UZSProfileSubsystem::PrestigeTier(Rank + 1), FText::AsNumber(UZSProfileSubsystem::MaxLevel),
						FText::AsNumber(FMath::RoundToInt(UZSProfileSubsystem::PrestigeXPScale * 100.f)), FText::FromString(Rewards));
					const bool bCan = Pr->CanPrestige();
					const FText Value = bCan ? NSLOCTEXT("ZS", "PrReady", "READY")
						: FText::Format(NSLOCTEXT("ZS", "PrLevelTag", "LEVEL {0}/{1}"), FText::AsNumber(Pr->GetLevel()), FText::AsNumber(UZSProfileSubsystem::MaxLevel));
					const FText Label = Rank > 0
						? FText::Format(NSLOCTEXT("ZS", "PrLabelRank", "PRESTIGE   Rank {0} ({1})  >  {2}"), FText::AsNumber(Rank), UZSProfileSubsystem::PrestigeTier(Rank), FText::AsNumber(Rank + 1))
						: NSLOCTEXT("ZS", "PrLabel", "PRESTIGE");
					P.Items.Emplace(Label, [WeakPC]()
					{
						UZSProfileSubsystem* Prof = ProfileOf(WeakPC);
						if (!Prof || !WeakPC.IsValid()) { return; }
						if (!Prof->CanPrestige())
						{
							FText Msg;
							Prof->DoPrestige(Msg);   // only builds the "reach level" message
							Say(WeakPC, Msg);
							return;
						}
						FZSMenuPage Confirm;
						Confirm.Title = NSLOCTEXT("ZS", "PrConfirmTitle", "PRESTIGE?");
						Confirm.Refresh = [WeakPC](FZSMenuPage& C)
						{
							const UZSProfileSubsystem* Q = ProfileOf(WeakPC);
							if (!Q) { return; }
							C.Subtitle = FText::Format(NSLOCTEXT("ZS", "PrConfirmSub", "Rank {0} ({1}). Level resets to 1. Weapons, cash, outfits and unlocks are kept."),
								FText::AsNumber(Q->GetPrestige() + 1), UZSProfileSubsystem::PrestigeTier(Q->GetPrestige() + 1));
							C.Items.Reset();
							C.Items.Emplace(NSLOCTEXT("ZS", "PrConfirm", "CONFIRM PRESTIGE"), [WeakPC]()
							{
								UZSProfileSubsystem* Z = ProfileOf(WeakPC);
								if (!Z || !WeakPC.IsValid()) { return; }
								FText Msg;
								Z->DoPrestige(Msg);
								WeakPC->PopMenu();
								Say(WeakPC, Msg);
							});
							C.Items.Add(BackItem(WeakPC));
						};
						WeakPC->PushMenu(Confirm);
					}, Value, Detail);
				}
			}
			const FText RuleText = NSLOCTEXT("ZS", "MissionRule", "Progress made during a run only counts if you WIN the run (kill the round-20 boss). Die or abandon and that run's progress is lost. Rewards are paid automatically.");
			for (const FZSMissionStatus& St : Missions->GetActive())
			{
				const TCHAR* Kind = St.Mission.Kind == EZSMissionKind::Daily ? TEXT("DAILY") : (St.Mission.Kind == EZSMissionKind::Weekly ? TEXT("WEEKLY") : TEXT("STORY"));
				const FText Label = FText::Format(NSLOCTEXT("ZS", "MissionLabel", "{0}   {1}"), FText::FromString(Kind), St.Mission.Title);
				const FText Value = St.bComplete ? NSLOCTEXT("ZS", "MissionDoneTag", "COMPLETE")
					: FText::Format(NSLOCTEXT("ZS", "MissionProgress", "{0} / {1}"), FText::AsNumber(St.Saved), FText::AsNumber(St.Mission.Target));
				const FText Detail = FText::Format(NSLOCTEXT("ZS", "MissionDetail", "Reward: {0} XP and ${1}\n\n{2}"),
					FText::AsNumber(St.Mission.RewardXP), FText::AsNumber(St.Mission.RewardCash), RuleText);
				P.Items.Emplace(Label, []() {}, Value, Detail);
			}
			if (Missions->GetStoryChapter() >= Missions->GetStoryChapterCount())
			{
				P.Items.Emplace(NSLOCTEXT("ZS", "StoryDone", "STORY   All chapters complete"), []() {}, NSLOCTEXT("ZS", "MissionDoneTag", "COMPLETE"));
			}
			P.Items.Add(BackItem(WeakPC));
		};
		PC->PushMenu(Page);
	}
}
