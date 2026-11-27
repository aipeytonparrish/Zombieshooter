#include "Weapons/ZSWeaponCatalog.h"
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
			E.Add({ TEXT("AR_M4"), AZSWeapon_TacticalAR::StaticClass(), NSLOCTEXT("ZS", "Cat_M4", "M4 Tactical"), true, true });
			E.Add({ TEXT("PST_P9"), AZSWeapon_Pistol::StaticClass(), NSLOCTEXT("ZS", "Cat_P9", "P9 Sidearm"), true, false });
			E.Add({ TEXT("AR_RD704"), AZSWeapon_RD704::StaticClass(), NSLOCTEXT("ZS", "Cat_RD704", "RD-704"), false, true });
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
}
