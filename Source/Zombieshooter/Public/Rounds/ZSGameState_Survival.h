#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Profile/ZSProfileTypes.h"
#include "Engine/World.h"
#include "ZSGameState_Survival.generated.h"

UENUM(BlueprintType)
enum class EZSRoundPhase : uint8
{
	PreGame,   // first seconds after spawning
	Active,    // zombies spawning / alive
	Break,     // between rounds
	GameOver   // run ended; results screen
};

/** A "+15 XP  $15" popup on the HUD. */
/** Timed / instant power-ups dropped by zombies (Milestone 11). */
UENUM(BlueprintType)
enum class EZSPowerUp : uint8
{
	None,
	Overkill,       // double damage
	OneShot,        // every hit kills
	ScorchedEarth,  // kills every zombie on the map (instant)
	Payday,         // double run XP and cash
	MAX UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct FZSRewardPopup
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Run") FText Text;
	UPROPERTY(BlueprintReadOnly, Category = "Run") float Time = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Run") bool bBig = false;
};

/** What the HUD and the results screen need to know about the run. Written by the survival game mode / round manager. */
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
	/** Round whose clear wins the run (the boss replaces this in Milestone 12). */
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") int32 WinRound = 20;

	// ---- Economy (at risk until the run is won) ----
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") int32 RunXP = 0;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") int32 RunCash = 0;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") TArray<FZSRewardPopup> Popups;
	/** Deployed gear a fail would cost (shown at deploy). */
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") TArray<FText> AtRisk;
	/** One-off message shown during the pre-game (e.g. "last run was abandoned"). */
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") FText Notice;

	// ---- Results ----
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") bool bShowResults = false;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") FZSRunResult Result;
	/** Profile cash after the run (results screen). */
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") int64 ProfileCash = 0;
	/** Seconds until the results screen continues on its own. */
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") float RestartIn = 0.f;

	// ---- Timed task (UZSRunTaskComponent) ----
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") int32 TasksCompleted = 0;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") bool bTaskActive = false;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") FText TaskTitle;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") FText TaskStatus;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") float TaskTimeLeft = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") float TaskProgress = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") bool bTaskHasMarker = false;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") FVector TaskMarker = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") float TaskStartTime = -100.f;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") FText TaskResult;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") float TaskResultTime = -100.f;

	// ---- Power-ups ----
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") FText PowerUpFlash;
	UPROPERTY(BlueprintReadOnly, Category = "ZS|Survival") float PowerUpFlashTime = -100.f;
	float PowerUpEnd[(int32)EZSPowerUp::MAX] = {};
	float PowerUpDuration[(int32)EZSPowerUp::MAX] = {};

	bool IsPowerUpActive(EZSPowerUp P) const
	{
		return GetWorld() && GetWorld()->GetTimeSeconds() < PowerUpEnd[(int32)P];
	}
	float PowerUpTimeLeft(EZSPowerUp P) const
	{
		return GetWorld() ? FMath::Max(0.f, PowerUpEnd[(int32)P] - GetWorld()->GetTimeSeconds()) : 0.f;
	}
	static FText PowerUpName(EZSPowerUp P)
	{
		switch (P)
		{
		case EZSPowerUp::Overkill:      return NSLOCTEXT("ZS", "PU_Overkill", "OVERKILL");
		case EZSPowerUp::OneShot:       return NSLOCTEXT("ZS", "PU_OneShot", "ONE SHOT");
		case EZSPowerUp::ScorchedEarth: return NSLOCTEXT("ZS", "PU_Scorched", "SCORCHED EARTH");
		case EZSPowerUp::Payday:        return NSLOCTEXT("ZS", "PU_Payday", "PAYDAY");
		default:                        return FText::GetEmpty();
		}
	}
	static FLinearColor PowerUpColor(EZSPowerUp P)
	{
		switch (P)
		{
		case EZSPowerUp::Overkill:      return FLinearColor(1.f, 0.15f, 0.1f);
		case EZSPowerUp::OneShot:       return FLinearColor(0.85f, 0.85f, 1.f);
		case EZSPowerUp::ScorchedEarth: return FLinearColor(1.f, 0.55f, 0.05f);
		case EZSPowerUp::Payday:        return FLinearColor(0.3f, 1.f, 0.35f);
		default:                        return FLinearColor::White;
		}
	}

	void AddPopup(const FText& Text, bool bBig = false)
	{
		FZSRewardPopup& P = Popups.AddDefaulted_GetRef();
		P.Text = Text;
		P.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		P.bBig = bBig;
		if (Popups.Num() > 6)
		{
			Popups.RemoveAt(0);
		}
	}
};
