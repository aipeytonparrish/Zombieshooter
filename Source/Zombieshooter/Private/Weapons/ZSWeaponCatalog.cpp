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
			A.Add({ TEXT("MUZ_SUPPRESSOR"), NSLOCTEXT("ZS", "Att_Supp", "Suppressor"), EZSAttachmentSlot::Muzzle, 700, 8,
				NSLOCTEXT("ZS", "Att_SuppD", "-10 % recoil, quieter, but -15 % damage range."),
				[](FZSWeaponStats& S) { for (FVector2D& R : S.RecoilPattern) { R *= 0.9f; } S.FalloffStart *= 0.85f; S.FalloffEnd *= 0.85f; S.MuzzlePoint.X += 16.f; } });
			A.Add({ TEXT("MUZ_BRAKE"), NSLOCTEXT("ZS", "Att_Brake", "Muzzle Brake"), EZSAttachmentSlot::Muzzle, 550, 4,
				NSLOCTEXT("ZS", "Att_BrakeD", "-18 % vertical recoil."),
				[](FZSWeaponStats& S) { for (FVector2D& R : S.RecoilPattern) { R.Y *= 0.82f; } } });
			A.Add({ TEXT("UB_VGRIP"), NSLOCTEXT("ZS", "Att_VGrip", "Vertical Grip"), EZSAttachmentSlot::Underbarrel, 500, 6,
				NSLOCTEXT("ZS", "Att_VGripD", "-12 % recoil, -10 % hip-fire spread."),
				[](FZSWeaponStats& S) { for (FVector2D& R : S.RecoilPattern) { R *= 0.88f; } S.HipSpread *= 0.9f; } });
			A.Add({ TEXT("MAG_EXT"), NSLOCTEXT("ZS", "Att_ExtMag", "Extended Mag"), EZSAttachmentSlot::Magazine, 650, 10,
				NSLOCTEXT("ZS", "Att_ExtMagD", "+50 % magazine size, reloads 10 % slower."),
				[](FZSWeaponStats& S) { S.MagSize = FMath::RoundToInt(S.MagSize * 1.5f); S.TacticalReloadTime *= 1.1f; S.EmptyReloadTime *= 1.1f; } });
			A.Add({ TEXT("STK_LIGHT"), NSLOCTEXT("ZS", "Att_Stock", "Light Stock"), EZSAttachmentSlot::Stock, 400, 12,
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

	const TArray<FCamo>& Camos()
	{
		static const TArray<FCamo> List = []()
		{
			TArray<FCamo> C;
			auto Add = [&C](const TCHAR* Id, FText Name, int32 Level, FLinearColor A, FLinearColor B, FLinearColor Cc, float Scale, float Metal = 0.f, float Emissive = 0.f)
			{
				C.Add({ FName(Id), Name, Level, A, B, Cc, Scale, Metal, Emissive });
			};
			Add(TEXT("NONE"), NSLOCTEXT("ZS", "CamoNone", "Factory"), 1, FLinearColor::Black, FLinearColor::Black, FLinearColor::Black, 1.f);
			Add(TEXT("WOODLAND"), NSLOCTEXT("ZS", "CamoWood", "Woodland"), 3, FLinearColor(0.02f, 0.025f, 0.012f), FLinearColor(0.07f, 0.09f, 0.03f), FLinearColor(0.16f, 0.13f, 0.07f), 1.f);
			Add(TEXT("DESERT"), NSLOCTEXT("ZS", "CamoDesert", "Desert"), 6, FLinearColor(0.2f, 0.14f, 0.07f), FLinearColor(0.36f, 0.27f, 0.15f), FLinearColor(0.52f, 0.43f, 0.28f), 1.3f);
			Add(TEXT("URBAN"), NSLOCTEXT("ZS", "CamoUrban", "Urban"), 9, FLinearColor(0.02f, 0.02f, 0.022f), FLinearColor(0.12f, 0.12f, 0.13f), FLinearColor(0.4f, 0.4f, 0.42f), 0.8f);
			Add(TEXT("ARCTIC"), NSLOCTEXT("ZS", "CamoArctic", "Arctic"), 12, FLinearColor(0.25f, 0.28f, 0.32f), FLinearColor(0.55f, 0.6f, 0.66f), FLinearColor(0.85f, 0.88f, 0.9f), 1.2f);
			Add(TEXT("TIGER"), NSLOCTEXT("ZS", "CamoTiger", "Tiger"), 15, FLinearColor(0.01f, 0.008f, 0.005f), FLinearColor(0.35f, 0.12f, 0.02f), FLinearColor(0.6f, 0.25f, 0.04f), 0.6f);
			Add(TEXT("CRIMSON"), NSLOCTEXT("ZS", "CamoCrimson", "Crimson"), 18, FLinearColor(0.03f, 0.0f, 0.0f), FLinearColor(0.2f, 0.01f, 0.01f), FLinearColor(0.45f, 0.03f, 0.02f), 0.9f);
			Add(TEXT("BLOODSOAKED"), NSLOCTEXT("ZS", "CamoBlood", "Blood-Soaked"), 21, FLinearColor(0.015f, 0.0f, 0.0f), FLinearColor(0.09f, 0.004f, 0.002f), FLinearColor(0.05f, 0.05f, 0.05f), 1.6f);
			Add(TEXT("GOLD"), NSLOCTEXT("ZS", "CamoGold", "Gold"), 25, FLinearColor(0.6f, 0.4f, 0.1f), FLinearColor(0.85f, 0.62f, 0.22f), FLinearColor(1.f, 0.8f, 0.4f), 2.f, 1.f);
			Add(TEXT("DARKMATTER"), NSLOCTEXT("ZS", "CamoDark", "Dark Matter"), 30, FLinearColor(0.01f, 0.0f, 0.02f), FLinearColor(0.08f, 0.0f, 0.15f), FLinearColor(0.3f, 0.05f, 0.6f), 0.7f, 0.3f, 2.f);
			return C;
		}();
		return List;
	}

	const FCamo* FindCamo(FName Id)
	{
		return Camos().FindByPredicate([Id](const FCamo& C) { return C.Id == Id; });
	}
}
