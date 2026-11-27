#include "Bunker/ZSBunkerMenus.h"
#include "Player/ZSPlayerController.h"
#include "Profile/ZSProfileSubsystem.h"
#include "Core/ZSLevelCatalog.h"
#include "Weapons/ZSWeaponCatalog.h"
#include "Weapons/ZSWeaponBase.h"
#include "UI/ZSHUD.h"
#include "Kismet/GameplayStatics.h"
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
				const bool bUnlocked = Profile->GetLevel() >= L.RequiredLevel;
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
			P.Subtitle = FText::Format(NSLOCTEXT("ZS", "WeaponSub", "{0}   Attachments {1}/{2}   (removing an attachment scraps it)"),
				CashLine(Profile), FText::AsNumber(W->Attachments.Num()), FText::AsNumber(ZSWeaponCatalog::MaxAttachments));
			for (const FName AttId : E->Attachments)
			{
				const ZSWeaponCatalog::FAttachment* A = ZSWeaponCatalog::FindAttachment(AttId);
				if (!A) { continue; }
				const bool bFitted = W->Attachments.Contains(AttId);
				const FText Why = Profile->CanFitAttachment(WeaponId, AttId);
				const FText Label = FText::Format(NSLOCTEXT("ZS", "AttLabel", "{0}: {1}"), ZSWeaponCatalog::SlotName(A->Slot), A->DisplayName);
				FText Value = bFitted ? NSLOCTEXT("ZS", "FittedTag", "FITTED - REMOVE")
					: (Profile->GetLevel() < A->RequiredLevel ? FText::Format(NSLOCTEXT("ZS", "LvlTag", "LEVEL {0}"), FText::AsNumber(A->RequiredLevel)) : Money(A->Price));
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
			for (const FZSInventoryWeapon& W : Profile->GetWeapons())
			{
				const FGuid Id = W.InstanceId;
				P.Items.Emplace(Profile->DescribeWeapon(Id), [WeakPC, Id]() { OpenWeaponPage(WeakPC, Id); },
					NSLOCTEXT("ZS", "Modify", "ATTACHMENTS / SELL"), StatCard(W.ItemId, W.Attachments));
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
				const bool bLocked = Profile->GetLevel() < E->RequiredLevel;
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
		PlaceholderPage(PC, NSLOCTEXT("ZS", "St_Character", "LOCKER"), NSLOCTEXT("ZS", "LockerSoon", "Character creator coming soon."));
	}

	void OpenMissions(AZSPlayerController* PC)
	{
		if (!PC) { return; }
		PlaceholderPage(PC, NSLOCTEXT("ZS", "St_Missions", "MISSION BOARD"), NSLOCTEXT("ZS", "MissionsSoon", "Missions coming soon."));
	}
}
