#include "Core/ZSGameMode_Sandbox.h"
#include "Player/ZSPlayerCharacter.h"
#include "Player/ZSPlayerController.h"
#include "World/ZSMovementCourse.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Zombieshooter.h"

AZSGameMode_Sandbox::AZSGameMode_Sandbox()
{
	DefaultPawnClass = AZSPlayerCharacter::StaticClass();
	PlayerControllerClass = AZSPlayerController::StaticClass();
}

void AZSGameMode_Sandbox::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UWorld* World = GetWorld();
	if (!World || !UGameplayStatics::HasOption(Options, TEXT("Course")))
	{
		return;
	}

	if (TActorIterator<AZSMovementCourse>(World))
	{
		return; // The level already has a course.
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform CourseTransform(FRotator::ZeroRotator, FVector::ZeroVector);
	if (AZSMovementCourse* Course = World->SpawnActorDeferred<AZSMovementCourse>(AZSMovementCourse::StaticClass(), CourseTransform))
	{
		// Spawned at runtime, so it can't be Static (SetMobility propagates to the child components).
		Course->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		Course->FinishSpawning(CourseTransform);
	}

	// Start point facing +X toward the lanes, matching Docs/EditorSetup.md.
	CourseStart = World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), FVector(-800.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
	UE_LOG(LogZS, Log, TEXT("Sandbox: spawned movement course at runtime (?Course)."));
}

AActor* AZSGameMode_Sandbox::ChoosePlayerStart_Implementation(AController* Player)
{
	if (CourseStart)
	{
		return CourseStart;
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}
