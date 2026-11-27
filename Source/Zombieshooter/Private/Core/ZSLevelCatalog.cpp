#include "Core/ZSLevelCatalog.h"

namespace ZSLevelCatalog
{
	const TArray<FEntry>& All()
	{
		static const TArray<FEntry> Entries = []()
		{
			TArray<FEntry> E;
			E.Add({ TEXT("TestCourse"), NSLOCTEXT("ZS", "Lvl_Test", "Training Yard"),
				NSLOCTEXT("ZS", "Lvl_TestDesc", "The graybox movement course outside the bunker. 18 spawn points, 3 ammo crates. Survive 20 rounds."),
				TEXT("/Game/ZS/Maps/Test/L_ZSTest?game=Survival"), 1 });
			E.Add({ TEXT("NewYork"), NSLOCTEXT("ZS", "Lvl_NY", "New York - Riverside Ruins"),
				NSLOCTEXT("ZS", "Lvl_NYDesc", "A dead New York street by the river on a grey, rain-soaked afternoon. Collapsed brownstones, burned-out cars, an abandoned bus and light fog off the water. Survive 20 rounds."),
				TEXT("/Game/ZS/Maps/L_NYC?game=Survival"), 1 });
			E.Add({ TEXT("Subway"), NSLOCTEXT("ZS", "Lvl_Subway", "Lexington Line"),
				NSLOCTEXT("ZS", "Lvl_SubwayDesc", "Flooded subway tunnels under the city. Unlocks at level 15."),
				TEXT(""), 15 });
			return E;
		}();
		return Entries;
	}

	const FEntry* Find(FName Id)
	{
		return All().FindByPredicate([Id](const FEntry& E) { return E.Id == Id; });
	}

	const TCHAR* BunkerURL()
	{
		return TEXT("/Game/ZS/Maps/L_Bunker");
	}
}
