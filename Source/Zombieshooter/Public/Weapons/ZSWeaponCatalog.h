#pragma once

#include "CoreMinimal.h"

class AZSWeaponBase;

/**
 * Maps the profile's weapon item ids to weapon classes. Becomes the FZSWeaponRow DataTable (prices, unlock levels,
 * attachment slots) when the market arrives in Milestone 8.
 */
namespace ZSWeaponCatalog
{
	struct FEntry
	{
		FName ItemId;
		TSubclassOf<AZSWeaponBase> Class;
		FText DisplayName;
		bool bStarter = false;      // always for sale; also auto-restocked until the market exists
		bool bPrimary = true;       // primary vs sidearm slot
	};

	ZOMBIESHOOTER_API const TArray<FEntry>& All();
	ZOMBIESHOOTER_API const FEntry* Find(FName ItemId);
	ZOMBIESHOOTER_API FText DisplayName(FName ItemId);
}
