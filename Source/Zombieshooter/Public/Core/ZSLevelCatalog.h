#pragma once

#include "CoreMinimal.h"

/**
 * Deployable levels and the player level that unlocks each (FZSLevelRow DataTable later). The map table in the
 * bunker lists all of them, locked ones greyed out with their required level.
 */
namespace ZSLevelCatalog
{
	struct FEntry
	{
		FName Id;
		FText DisplayName;
		FText Description;
		/** Map URL; empty = not built yet (shown as "coming soon"). */
		FString MapURL;
		int32 RequiredLevel = 1;
	};

	ZOMBIESHOOTER_API const TArray<FEntry>& All();
	ZOMBIESHOOTER_API const FEntry* Find(FName Id);

	/** The bunker (home base) map. */
	ZOMBIESHOOTER_API const TCHAR* BunkerURL();
}
