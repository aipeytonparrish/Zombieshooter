#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ZSGameState_Survival.generated.h"

UENUM(BlueprintType)
enum class EZSRoundPhase : uint8
{
	PreGame,   // first seconds after spawning
	Active,    // zombies spawning / alive
	Break,     // between rounds
	GameOver
};

/** What the HUD (and later the results screen) needs to know about the run. Written by UZSRoundManagerComponent. */
UCLASS()
class ZOMBIESHOOTER_API AZSGameState_Survival : public AGameStateBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") int32 Round = 0;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") EZSRoundPhase Phase = EZSRoundPhase::PreGame;
	/** Seconds left in the current pre-game / break. */
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") float PhaseTimeLeft = 0.f;
	/** Zombies still to kill this round (alive + not yet spawned). */
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") int32 ZombiesRemaining = 0;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") int32 Kills = 0;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") int32 Headshots = 0;
	/** Seconds since the run started. */
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") float RunTime = 0.f;
	/** World time the current round began (HUD banner). */
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") float RoundStartTime = -100.f;
	/** Seconds until the automatic restart after game over. */
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") float RestartIn = 0.f;
};
