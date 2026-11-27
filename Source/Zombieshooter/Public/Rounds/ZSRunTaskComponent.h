#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZSRunTaskComponent.generated.h"

class AZSTaskMarker;

UENUM()
enum class EZSRunTaskType : uint8
{
	None,
	KillRush,   // kill N zombies before the timer runs out
	Defend,     // stay inside a zone for T seconds (total) before the timer runs out
	Reach       // get to a beacon before the timer runs out
};

/**
 * Optional timed tasks during a survival run (lives on the survival game mode). A task offers itself a few seconds into
 * certain rounds; completing it pays run XP / cash (at risk like all run earnings) and counts for missions.
 * State is mirrored to AZSGameState_Survival for the HUD.
 */
UCLASS()
class ZOMBIESHOOTER_API UZSRunTaskComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZSRunTaskComponent();

	/** Rounds that start a task (every 3rd round by default). */
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Tasks") int32 TaskEveryNRounds = 3;
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Tasks") float StartDelay = 6.f;

	void OnRoundStarted(int32 Round);
	void OnZombieKilled();
	void EndRun();

	/** Debug / tests: start a task of this type now. */
	void StartTask(EZSRunTaskType Type);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void Finish(bool bSuccess);
	bool PickMarkerLocation(float MinDist, float MaxDist, FVector& Out) const;
	void Publish();

	EZSRunTaskType Type = EZSRunTaskType::None;
	int32 Round = 0;
	float PendingStart = -1.f;
	float TimeLeft = 0.f;
	int32 Goal = 0;
	int32 Count = 0;
	float ZoneTime = 0.f;
	float ZoneRadius = 380.f;

	UPROPERTY(Transient)
	TObjectPtr<AZSTaskMarker> Marker;
};
