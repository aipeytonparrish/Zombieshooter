#pragma once

#include "CoreMinimal.h"
#include "Core/ZSGameMode_Sandbox.h"
#include "ZSBunkerGameMode.generated.h"

/**
 * The hub between runs: no weapons, no zombies. The player walks between stations (map table, armory, market,
 * locker, mission board) and deploys from the map table. Set as L_Bunker's GameMode Override, or "?game=Bunker".
 */
UCLASS()
class ZOMBIESHOOTER_API AZSBunkerGameMode : public AZSGameMode_Sandbox
{
	GENERATED_BODY()

public:
	AZSBunkerGameMode();

	virtual void BeginPlay() override;

	/** One-off message shown when the bunker opens (e.g. "your last run was force-quit"). */
	FText Notice;
	float NoticeTime = 0.f;
};
