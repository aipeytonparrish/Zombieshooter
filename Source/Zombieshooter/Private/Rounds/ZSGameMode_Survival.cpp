#include "Rounds/ZSGameMode_Survival.h"
#include "Rounds/ZSRoundManagerComponent.h"
#include "Rounds/ZSGameState_Survival.h"
#include "Player/ZSPlayerCharacter.h"
#include "Engine/World.h"
#include "Zombieshooter.h"

AZSGameMode_Survival::AZSGameMode_Survival()
{
	PrimaryActorTick.bCanEverTick = true;
	GameStateClass = AZSGameState_Survival::StaticClass();
	RoundManager = CreateDefaultSubobject<UZSRoundManagerComponent>(TEXT("RoundManager"));
}

bool AZSGameMode_Survival::HandlePlayerDeath(AZSPlayerCharacter* Player)
{
	RoundManager->EndRun();
	RestartTimer = RestartDelay;
	if (const AZSGameState_Survival* GS = GetGameState<AZSGameState_Survival>())
	{
		UE_LOG(LogZS, Log, TEXT("GAME OVER: reached round %d, %d kills (%d headshots), %.0f s."), GS->Round, GS->Kills, GS->Headshots, GS->RunTime);
	}
	return true;   // no sandbox respawn
}

void AZSGameMode_Survival::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (RestartTimer > 0.f)
	{
		RestartTimer -= DeltaSeconds;
		if (AZSGameState_Survival* GS = GetGameState<AZSGameState_Survival>())
		{
			GS->RestartIn = FMath::Max(0.f, RestartTimer);
		}
		if (RestartTimer <= 0.f)
		{
			RestartRun();
		}
	}
}

void AZSGameMode_Survival::RestartRun()
{
	RestartTimer = -1.f;
	// Reloads the current map with the same options (keeps ?Course etc.).
	GetWorld()->ServerTravel(TEXT("?Restart"), false);
}
