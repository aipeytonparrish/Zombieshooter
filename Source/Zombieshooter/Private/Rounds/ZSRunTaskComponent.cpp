#include "Rounds/ZSRunTaskComponent.h"
#include "Rounds/ZSGameMode_Survival.h"
#include "Rounds/ZSGameState_Survival.h"
#include "Missions/ZSMissionSubsystem.h"
#include "World/ZSTaskMarker.h"
#include "World/ZSSpawnPoint.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Zombieshooter.h"

UZSRunTaskComponent::UZSRunTaskComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UZSRunTaskComponent::OnRoundStarted(int32 InRound)
{
	Round = InRound;
	if (Type == EZSRunTaskType::None && TaskEveryNRounds > 0 && InRound % TaskEveryNRounds == 0)
	{
		PendingStart = StartDelay;
	}
}

void UZSRunTaskComponent::OnZombieKilled()
{
	if (Type == EZSRunTaskType::KillRush)
	{
		++Count;
		if (Count >= Goal)
		{
			Finish(true);
		}
		else
		{
			Publish();
		}
	}
}

void UZSRunTaskComponent::EndRun()
{
	PendingStart = -1.f;
	if (Type != EZSRunTaskType::None)
	{
		Type = EZSRunTaskType::None;
		if (Marker) { Marker->Destroy(); Marker = nullptr; }
		Publish();
	}
}

bool UZSRunTaskComponent::PickMarkerLocation(float MinDist, float MaxDist, FVector& Out) const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return false;
	}
	const FVector From = Player->GetActorLocation();
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		for (int32 Try = 0; Try < 30; ++Try)
		{
			FNavLocation P;
			if (Nav->GetRandomReachablePointInRadius(From, MaxDist, P))
			{
				const float D = FVector::Dist2D(P.Location, From);
				if (D >= MinDist)
				{
					Out = P.Location;
					return true;
				}
			}
		}
	}
	// Fallback: the farthest-enough spawn point.
	for (TActorIterator<AZSSpawnPoint> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist2D(It->GetActorLocation(), From);
		if (D >= MinDist && D <= MaxDist * 1.5f)
		{
			Out = It->GetActorLocation() - FVector(0.f, 0.f, 90.f);
			return true;
		}
	}
	return false;
}

void UZSRunTaskComponent::StartTask(EZSRunTaskType NewType)
{
	PendingStart = -1.f;
	if (Marker) { Marker->Destroy(); Marker = nullptr; }
	Type = NewType;
	Count = 0;
	ZoneTime = 0.f;
	const int32 R = FMath::Max(1, Round);

	FVector Loc;
	switch (Type)
	{
	case EZSRunTaskType::KillRush:
		Goal = 8 + R;
		TimeLeft = 40.f + R * 0.5f;
		break;
	case EZSRunTaskType::Defend:
		if (!PickMarkerLocation(600.f, 2200.f, Loc)) { Type = EZSRunTaskType::KillRush; StartTask(Type); return; }
		Goal = 20;   // seconds inside the zone
		TimeLeft = 60.f;
		break;
	case EZSRunTaskType::Reach:
		if (!PickMarkerLocation(1800.f, 4000.f, Loc)) { Type = EZSRunTaskType::KillRush; StartTask(Type); return; }
		Goal = 1;
		TimeLeft = 30.f;
		break;
	default:
		return;
	}

	if (Type == EZSRunTaskType::Defend || Type == EZSRunTaskType::Reach)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Marker = GetWorld()->SpawnActor<AZSTaskMarker>(AZSTaskMarker::StaticClass(), FTransform(Loc), Params);
		if (Marker)
		{
			Marker->Setup(Type == EZSRunTaskType::Defend ? ZoneRadius : 150.f,
				Type == EZSRunTaskType::Defend ? FLinearColor(1.f, 0.55f, 0.1f) : FLinearColor(0.2f, 0.8f, 1.f));
		}
	}
	UE_LOG(LogZS, Log, TEXT("Task started: type %d goal %d time %.0f"), (int32)Type, Goal, TimeLeft);
	if (AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>())
	{
		GS->TaskResult = FText::GetEmpty();
		GS->TaskStartTime = GetWorld()->GetTimeSeconds();
	}
	Publish();
}

void UZSRunTaskComponent::Finish(bool bSuccess)
{
	AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>();
	AZSGameMode_Survival* GM = Cast<AZSGameMode_Survival>(GetOwner());
	const int32 R = FMath::Max(1, Round);
	if (bSuccess && GM && GS)
	{
		GS->TasksCompleted++;
		GM->AddReward(150 + 25 * R, 100 + 10 * R, NSLOCTEXT("ZS", "TaskDone", "TASK COMPLETE"), true);
		if (UZSMissionSubsystem* Missions = UZSMissionSubsystem::Get(this)) { Missions->Record(EZSMissionStat::TasksCompleted); }
	}
	if (GS)
	{
		GS->TaskResult = bSuccess ? NSLOCTEXT("ZS", "TaskComplete", "TASK COMPLETE") : NSLOCTEXT("ZS", "TaskFailed", "TASK FAILED");
		GS->TaskResultTime = GetWorld()->GetTimeSeconds();
	}
	Type = EZSRunTaskType::None;
	if (Marker) { Marker->Destroy(); Marker = nullptr; }
	Publish();
}

void UZSRunTaskComponent::Publish()
{
	AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>();
	if (!GS)
	{
		return;
	}
	GS->bTaskActive = Type != EZSRunTaskType::None;
	GS->TaskTimeLeft = TimeLeft;
	GS->bTaskHasMarker = Marker != nullptr;
	GS->TaskMarker = Marker ? Marker->GetActorLocation() : FVector::ZeroVector;
	switch (Type)
	{
	case EZSRunTaskType::KillRush:
		GS->TaskTitle = NSLOCTEXT("ZS", "TaskKill", "KILL RUSH");
		GS->TaskStatus = FText::Format(NSLOCTEXT("ZS", "TaskKillSt", "Kill {0} zombies   {1} / {0}"), FText::AsNumber(Goal), FText::AsNumber(Count));
		GS->TaskProgress = static_cast<float>(Count) / FMath::Max(1, Goal);
		break;
	case EZSRunTaskType::Defend:
		GS->TaskTitle = NSLOCTEXT("ZS", "TaskDefend", "HOLD THE ZONE");
		GS->TaskStatus = FText::Format(NSLOCTEXT("ZS", "TaskDefendSt", "Stay inside the orange ring   {0} s left to hold"), FText::AsNumber(FMath::CeilToInt(Goal - ZoneTime)));
		GS->TaskProgress = ZoneTime / FMath::Max(1, Goal);
		break;
	case EZSRunTaskType::Reach:
		GS->TaskTitle = NSLOCTEXT("ZS", "TaskReach", "SUPPLY BEACON");
		GS->TaskStatus = NSLOCTEXT("ZS", "TaskReachSt", "Get to the blue beacon");
		GS->TaskProgress = 0.f;
		break;
	default:
		break;
	}
}

void UZSRunTaskComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (PendingStart > 0.f)
	{
		PendingStart -= DeltaTime;
		if (PendingStart <= 0.f)
		{
			// Rotate through the types so a run sees all of them.
			const int32 Pick = (Round / FMath::Max(1, TaskEveryNRounds)) % 3;
			StartTask(Pick == 1 ? EZSRunTaskType::KillRush : (Pick == 2 ? EZSRunTaskType::Defend : EZSRunTaskType::Reach));
		}
		return;
	}
	if (Type == EZSRunTaskType::None)
	{
		return;
	}

	TimeLeft -= DeltaTime;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Player && Marker)
	{
		const float D = FVector::Dist2D(Player->GetActorLocation(), Marker->GetActorLocation());
		const float DZ = FMath::Abs(Player->GetActorLocation().Z - Marker->GetActorLocation().Z);
		if (Type == EZSRunTaskType::Defend && D <= ZoneRadius && DZ < 300.f)
		{
			ZoneTime += DeltaTime;
			if (ZoneTime >= Goal) { Finish(true); return; }
		}
		else if (Type == EZSRunTaskType::Reach && D <= 170.f && DZ < 300.f)
		{
			Finish(true);
			return;
		}
	}
	if (TimeLeft <= 0.f)
	{
		Finish(false);
		return;
	}
	Publish();
}
