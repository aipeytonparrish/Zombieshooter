#include "Rounds/ZSGameMode_Survival.h"
#include "Rounds/ZSRoundManagerComponent.h"
#include "Rounds/ZSRunTaskComponent.h"
#include "Missions/ZSMissionSubsystem.h"
#include "Rounds/ZSGameState_Survival.h"
#include "Profile/ZSProfileSubsystem.h"
#include "Player/ZSPlayerCharacter.h"
#include "Weapons/ZSWeaponCatalog.h"
#include "Weapons/ZSWeaponBase.h"
#include "Zombies/ZSZombieBase.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Core/ZSLevelCatalog.h"
#include "Misc/PackageName.h"
#include "EngineUtils.h"
#include "Combat/ZSHealthComponent.h"
#include "Zombieshooter.h"

AZSGameMode_Survival::AZSGameMode_Survival()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	GameStateClass = AZSGameState_Survival::StaticClass();
	RoundManager = CreateDefaultSubobject<UZSRoundManagerComponent>(TEXT("RoundManager"));
	Tasks = CreateDefaultSubobject<UZSRunTaskComponent>(TEXT("Tasks"));
}

void AZSGameMode_Survival::BeginPlay()
{
	Super::BeginPlay();
	EnsureDeployed();

	if (AZSGameState_Survival* GS = GetGameState<AZSGameState_Survival>())
	{
		FZSRunResult Previous;
		if (UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this); Profile && Profile->ConsumeForceQuitNotice(Previous))
		{
			FString Lost;
			for (const FText& T : Previous.LostItems)
			{
				Lost += (Lost.IsEmpty() ? TEXT("") : TEXT(", ")) + T.ToString();
			}
			GS->Notice = FText::Format(NSLOCTEXT("ZS", "ForceQuitNotice", "Your last run was quit mid-match and counts as a loss{0}"),
				Lost.IsEmpty() ? FText::GetEmpty() : FText::FromString(FString::Printf(TEXT(" - lost: %s"), *Lost)));
		}
	}
}

void AZSGameMode_Survival::EnsureDeployed()
{
	if (bDeployed)
	{
		return;
	}
	bDeployed = true;
	if (UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this))
	{
		Deployed = Profile->BeginRun();
		if (UZSMissionSubsystem* Missions = UZSMissionSubsystem::Get(this)) { Missions->BeginRun(); }
		if (AZSGameState_Survival* GS = GetGameState<AZSGameState_Survival>())
		{
			GS->AtRisk = Profile->GetAtRiskNames();
		}
	}
}

TArray<FZSInventoryWeapon> AZSGameMode_Survival::GetDeployedWeapons()
{
	EnsureDeployed();
	return Deployed;
}

// ---- Economy --------------------------------------------------------------------------------

void AZSGameMode_Survival::AddReward(int32 XP, int32 Cash, const FText& Label, bool bBig)
{
	AZSGameState_Survival* GS = GetGameState<AZSGameState_Survival>();
	if (!GS || bRunEnded)
	{
		return;
	}
	if (GS->IsPowerUpActive(EZSPowerUp::Payday))
	{
		XP *= 2;
		Cash *= 2;
	}
	GS->RunXP += XP;
	GS->RunCash += Cash;
	const FText Line = Label.IsEmpty()
		? FText::Format(NSLOCTEXT("ZS", "RewardLine", "+{0} XP   ${1}"), FText::AsNumber(XP), FText::AsNumber(Cash))
		: FText::Format(NSLOCTEXT("ZS", "RewardLineLabel", "{0}  +{1} XP   ${2}"), Label, FText::AsNumber(XP), FText::AsNumber(Cash));
	GS->AddPopup(Line, bBig);
}

void AZSGameMode_Survival::RewardKill(AZSZombieBase* Zombie, bool bHeadshot)
{
	const float Now = GetWorld()->GetTimeSeconds();
	KillStreak = (Now - LastKillTime <= MultiKillWindow) ? KillStreak + 1 : 1;
	LastKillTime = Now;

	const bool bRunner = Zombie && Zombie->ZombieType == EZSZombieType::Runner;
	const bool bSpecial = Zombie && Zombie->IsSpecial();
	const bool bBrute = Zombie && Zombie->ZombieType == EZSZombieType::Brute;
	if (UZSMissionSubsystem* Missions = UZSMissionSubsystem::Get(this))
	{
		Missions->Record(EZSMissionStat::Kills);
		if (bHeadshot) { Missions->Record(EZSMissionStat::Headshots); }
		if (bRunner) { Missions->Record(EZSMissionStat::RunnerKills); }
		if (KillStreak >= 2) { Missions->Record(EZSMissionStat::MultiKills); }
	}
	Tasks->OnZombieKilled();
	const int32 SpecialBonus = bBrute ? 150 : (bSpecial ? 20 : 0);
	int32 XP = KillXP + (bRunner ? RunnerBonus : 0) + (bHeadshot ? HeadshotBonus : 0) + SpecialBonus;
	const int32 Cash = KillCash + (bRunner ? RunnerBonus : 0) + (bHeadshot ? HeadshotBonus : 0) + SpecialBonus;
	FText Label;
	if (bSpecial)
	{
		Label = FText::Format(NSLOCTEXT("ZS", "SpecialKilled", "{0} KILLED"), AZSZombieBase::TypeName(Zombie->ZombieType));
	}
	if (!bSpecial && KillStreak >= 2)
	{
		XP += MultiKillXP * (KillStreak - 1);
		Label = KillStreak == 2 ? NSLOCTEXT("ZS", "DoubleKill", "DOUBLE KILL") : FText::Format(NSLOCTEXT("ZS", "MultiKill", "{0}x MULTI KILL"), FText::AsNumber(KillStreak));
	}
	else if (!bSpecial && bHeadshot)
	{
		Label = NSLOCTEXT("ZS", "Headshot", "HEADSHOT");
	}
	AddReward(XP, Cash, Label);
}

void AZSGameMode_Survival::OnRoundStarted(int32 Round)
{
	Tasks->OnRoundStarted(Round);
}

void AZSGameMode_Survival::OnRoundCleared(int32 Round)
{
	if (UZSMissionSubsystem* Missions = UZSMissionSubsystem::Get(this))
	{
		Missions->Record(EZSMissionStat::RoundsSurvived);
		Missions->Record(EZSMissionStat::ReachRound, Round);
	}
	AddReward(RoundXPBase + RoundXPPerRound * Round, RoundCashPerRound * Round,
		FText::Format(NSLOCTEXT("ZS", "RoundBonus", "ROUND {0} SURVIVED"), FText::AsNumber(Round)), true);

	const AZSGameState_Survival* GS = GetGameState<AZSGameState_Survival>();
	if (GS && Round >= GS->WinRound)
	{
		EndRun(EZSRunOutcome::Won);
	}
}

// ---- Run end --------------------------------------------------------------------------------

bool AZSGameMode_Survival::HandlePlayerDeath(AZSPlayerCharacter* Player)
{
	EndRun(EZSRunOutcome::Died);
	return true;   // no sandbox respawn
}

void AZSGameMode_Survival::AbandonRun()
{
	EndRun(EZSRunOutcome::Abandoned);
	if (AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Char->DisableInput(UGameplayStatics::GetPlayerController(this, 0));
	}
}

void AZSGameMode_Survival::EndRun(EZSRunOutcome Outcome)
{
	if (bRunEnded)
	{
		return;
	}
	bRunEnded = true;
	RoundManager->EndRun();

	AZSGameState_Survival* GS = GetGameState<AZSGameState_Survival>();
	UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this);
	if (GS && Profile)
	{
		FZSRunStats Stats;
		Stats.RoundReached = GS->Round;
		Stats.Kills = GS->Kills;
		Stats.Headshots = GS->Headshots;
		Stats.RunTime = GS->RunTime;
		Stats.XPEarned = GS->RunXP;
		Stats.CashEarned = GS->RunCash;
		Stats.TasksCompleted = GS->TasksCompleted;
		Tasks->EndRun();
		GS->Result = Profile->EndRun(Outcome, Stats);
		if (UZSMissionSubsystem* Missions = UZSMissionSubsystem::Get(this))
		{
			GS->Result.Unlocks.Append(Missions->EndRun(Outcome == EZSRunOutcome::Won));
		}
		GS->ProfileCash = Profile->GetCash();
		GS->bShowResults = true;
		GS->Phase = EZSRoundPhase::GameOver;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetPause(false);
	}
	ResultsTimer = ResultsAutoContinue;
}

void AZSGameMode_Survival::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ResultsTimer > 0.f)
	{
		ResultsTimer -= DeltaSeconds;
		if (AZSGameState_Survival* GS = GetGameState<AZSGameState_Survival>())
		{
			GS->RestartIn = FMath::Max(0.f, ResultsTimer);
		}
		if (ResultsTimer <= 0.f)
		{
			RestartRun();
		}
	}
}

void AZSGameMode_Survival::RestartRun()
{
	ResultsTimer = -1.f;
	const FString Bunker = ZSLevelCatalog::BunkerURL();
	if (FPackageName::DoesPackageExist(Bunker))
	{
		UGameplayStatics::OpenLevel(this, FName(*Bunker));
		return;
	}
	GetWorld()->ServerTravel(TEXT("?Restart"), false);
}

void AZSGameMode_Survival::ActivatePowerUp(EZSPowerUp PowerUp)
{
	AZSGameState_Survival* GS = GetGameState<AZSGameState_Survival>();
	if (!GS || bRunEnded)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	float Duration = 0.f;
	switch (PowerUp)
	{
	case EZSPowerUp::Overkill: Duration = 20.f; break;
	case EZSPowerUp::OneShot:  Duration = 15.f; break;
	case EZSPowerUp::Payday:   Duration = 30.f; break;
	case EZSPowerUp::ScorchedEarth:
	{
		// Everything on the map dies, credited to the player (normal kill rewards apply).
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		int32 Killed = 0;
		for (TActorIterator<AZSZombieBase> It(GetWorld()); It; ++It)
		{
			if (!It->IsDead())
			{
				It->GetHealth()->Kill(PC, PC ? PC->GetPawn() : nullptr);
				++Killed;
			}
		}
		UE_LOG(LogZS, Log, TEXT("Scorched Earth killed %d zombies."), Killed);
		break;
	}
	default: break;
	}
	GS->PowerUpEnd[(int32)PowerUp] = Now + Duration;
	GS->PowerUpDuration[(int32)PowerUp] = Duration;
	GS->AddPopup(AZSGameState_Survival::PowerUpName(PowerUp), true);
	GS->Notice = FText::GetEmpty();
	GS->PowerUpFlash = AZSGameState_Survival::PowerUpName(PowerUp);
	GS->PowerUpFlashTime = Now;
}
