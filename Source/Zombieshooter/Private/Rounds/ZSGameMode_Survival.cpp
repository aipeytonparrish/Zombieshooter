#include "Rounds/ZSGameMode_Survival.h"
#include "Rounds/ZSRoundManagerComponent.h"
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
#include "Zombieshooter.h"

AZSGameMode_Survival::AZSGameMode_Survival()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	GameStateClass = AZSGameState_Survival::StaticClass();
	RoundManager = CreateDefaultSubobject<UZSRoundManagerComponent>(TEXT("RoundManager"));
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
	int32 XP = KillXP + (bRunner ? RunnerBonus : 0) + (bHeadshot ? HeadshotBonus : 0);
	const int32 Cash = KillCash + (bRunner ? RunnerBonus : 0) + (bHeadshot ? HeadshotBonus : 0);
	FText Label;
	if (KillStreak >= 2)
	{
		XP += MultiKillXP * (KillStreak - 1);
		Label = KillStreak == 2 ? NSLOCTEXT("ZS", "DoubleKill", "DOUBLE KILL") : FText::Format(NSLOCTEXT("ZS", "MultiKill", "{0}x MULTI KILL"), FText::AsNumber(KillStreak));
	}
	else if (bHeadshot)
	{
		Label = NSLOCTEXT("ZS", "Headshot", "HEADSHOT");
	}
	AddReward(XP, Cash, Label);
}

void AZSGameMode_Survival::OnRoundCleared(int32 Round)
{
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
		GS->Result = Profile->EndRun(Outcome, Stats);
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
