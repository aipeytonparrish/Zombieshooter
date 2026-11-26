#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZSAimAssistSubsystem.generated.h"

/** One thing aim assist can pull toward. Zombies register in Milestone 4; test dummies register now. */
USTRUCT()
struct FZSAimTarget
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;

	/** Centre-mass point. */
	UPROPERTY()
	TWeakObjectPtr<USceneComponent> Body;

	/** Optional head point (preferred by ADS snap when close). */
	UPROPERTY()
	TWeakObjectPtr<USceneComponent> Head;

	/** Rough target radius in cm (bigger targets get a slightly bigger assist cone). */
	float Radius = 35.f;
};

/** Registry of aim-assist targets so the weapon never iterates every actor in the world. */
UCLASS()
class ZOMBIESHOOTER_API UZSAimAssistSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterTarget(AActor* Actor, USceneComponent* Body, USceneComponent* Head, float Radius);
	void UnregisterTarget(AActor* Actor);

	/** Valid targets (stale entries are pruned). */
	const TArray<FZSAimTarget>& GetTargets();

private:
	UPROPERTY()
	TArray<FZSAimTarget> Targets;
};
