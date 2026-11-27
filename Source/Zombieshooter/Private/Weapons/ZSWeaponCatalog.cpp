#include "Weapons/ZSWeaponCatalog.h"
#include "Weapons/ZSWeaponTypes.h"
#include "Weapons/ZSWeapon_TacticalAR.h"
#include "Weapons/ZSWeapon_Pistol.h"
#include "Weapons/ZSWeapon_RD704.h"

namespace ZSWeaponCatalog
{
	const TArray<FEntry>& All()
	{
		static const TArray<FEntry> Entries = []()
		{
			TArray<FEntry> E;
			E.Add({ TEXT("PST_P9"), AZSWeapon_Pistol::StaticClass(), NSLOCTEXT("ZS", "Cat_P9", "P9 Sidearm"), true, false, 250, 1,
				{ TEXT("MUZ_SUPPRESSOR"), TEXT("MAG_EXT") },
				NSLOCTEXT("ZS", "Cat_P9D", "Reliable semi-auto sidearm. Cheap to replace, 12 rounds, fast handling.") });
			E.Add({ TEXT("AR_M4"), AZSWeapon_TacticalAR::StaticClass(), NSLOCTEXT("ZS", "Cat_M4", "M4 Tactical"), true, true, 900, 1,
				{ TEXT("OPT_REDDOT"), TEXT("MUZ_SUPPRESSOR"), TEXT("MUZ_BRAKE"), TEXT("UB_VGRIP"), TEXT("MAG_EXT"), TEXT("STK_LIGHT") },
				NSLOCTEXT("ZS", "Cat_M4D", "Versatile 5.56 rifle. Auto / burst / semi, 30 rounds, controllable recoil.") });
			E.Add({ TEXT("AR_RD704"), AZSWeapon_RD704::StaticClass(), NSLOCTEXT("ZS", "Cat_RD704", "RD-704"), false, true, 2400, 4,
				{ TEXT("MUZ_SUPPRESSOR"), TEXT("MUZ_BRAKE"), TEXT("UB_VGRIP"), TEXT("MAG_EXT"), TEXT("STK_LIGHT") },
				NSLOCTEXT("ZS", "Cat_RD704D", "Hard-hitting 7.62 AK platform with a Bravo4 optic. Punches through thin walls and zombie crowds.") });
			return E;
		}();
		return Entries;
	}

	const FEntry* Find(FName ItemId)
	{
		return All().FindByPredicate([ItemId](const FEntry& E) { return E.ItemId == ItemId; });
	}

	FText DisplayName(FName ItemId)
	{
		const FEntry* E = Find(ItemId);
		return E ? E->DisplayName : FText::FromName(ItemId);
	}

	const TArray<FAttachment>& Attachments()
	{
		static const TArray<FAttachment> List = []()
		{
			TArray<FAttachment> A;
			A.Add({ TEXT("OPT_REDDOT"), NSLOCTEXT("ZS", "Att_RedDot", "Reflex Sight"), EZSAttachmentSlot::Optic, 450, 2,
				NSLOCTEXT("ZS", "Att_RedDotD", "Faster aim-down-sights (-12 %) and a little more zoom."),
				[](FZSWeaponStats& S) { S.ADSTime *= 0.88f; S.ADSFOVMultiplier = FMath::Min(S.ADSFOVMultiplier, 0.78f); } });
			A.Add({ TEXT("MUZ_SUPPRESSOR"), NSLOCTEXT("ZS", "Att_Supp", "Suppressor"), EZSAttachmentSlot::Muzzle, 700, 3,
				NSLOCTEXT("ZS", "Att_SuppD", "-10 % recoil, quieter, but -15 % damage range."),
				[](FZSWeaponStats& S) { for (FVector2D& R : S.RecoilPattern) { R *= 0.9f; } S.FalloffStart *= 0.85f; S.FalloffEnd *= 0.85f; S.MuzzlePoint.X += 16.f; } });
			A.Add({ TEXT("MUZ_BRAKE"), NSLOCTEXT("ZS", "Att_Brake", "Muzzle Brake"), EZSAttachmentSlot::Muzzle, 550, 2,
				NSLOCTEXT("ZS", "Att_BrakeD", "-18 % vertical recoil."),
				[](FZSWeaponStats& S) { for (FVector2D& R : S.RecoilPattern) { R.Y *= 0.82f; } } });
			A.Add({ TEXT("UB_VGRIP"), NSLOCTEXT("ZS", "Att_VGrip", "Vertical Grip"), EZSAttachmentSlot::Underbarrel, 500, 2,
				NSLOCTEXT("ZS", "Att_VGripD", "-12 % recoil, -10 % hip-fire spread."),
				[](FZSWeaponStats& S) { for (FVector2D& R : S.RecoilPattern) { R *= 0.88f; } S.HipSpread *= 0.9f; } });
			A.Add({ TEXT("MAG_EXT"), NSLOCTEXT("ZS", "Att_ExtMag", "Extended Mag"), EZSAttachmentSlot::Magazine, 650, 5,
				NSLOCTEXT("ZS", "Att_ExtMagD", "+50 % magazine size, reloads 10 % slower."),
				[](FZSWeaponStats& S) { S.MagSize = FMath::RoundToInt(S.MagSize * 1.5f); S.TacticalReloadTime *= 1.1f; S.EmptyReloadTime *= 1.1f; } });
			A.Add({ TEXT("STK_LIGHT"), NSLOCTEXT("ZS", "Att_Stock", "Light Stock"), EZSAttachmentSlot::Stock, 400, 6,
				NSLOCTEXT("ZS", "Att_StockD", "-15 % aim-down-sights time, -15 % moving spread, +5 % recoil."),
				[](FZSWeaponStats& S) { S.ADSTime *= 0.85f; S.MoveSpread *= 0.85f; for (FVector2D& R : S.RecoilPattern) { R *= 1.05f; } } });
			return A;
		}();
		return List;
	}

	const FAttachment* FindAttachment(FName Id)
	{
		return Attachments().FindByPredicate([Id](const FAttachment& A) { return A.Id == Id; });
	}

	FText SlotName(EZSAttachmentSlot Slot)
	{
		switch (Slot)
		{
		case EZSAttachmentSlot::Optic:       return NSLOCTEXT("ZS", "Slot_Optic", "Optic");
		case EZSAttachmentSlot::Muzzle:      return NSLOCTEXT("ZS", "Slot_Muzzle", "Muzzle");
		case EZSAttachmentSlot::Underbarrel: return NSLOCTEXT("ZS", "Slot_Under", "Underbarrel");
		case EZSAttachmentSlot::Magazine:    return NSLOCTEXT("ZS", "Slot_Mag", "Magazine");
		case EZSAttachmentSlot::Stock:       return NSLOCTEXT("ZS", "Slot_Stock", "Stock");
		}
		return FText::GetEmpty();
	}
}
