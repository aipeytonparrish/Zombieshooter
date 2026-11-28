#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZSGoreSubsystem.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * Milestone 11 gore on top of UZSBloodSubsystem: flesh gibs (pooled physics chunks), blood spurting from severed
 * limbs, and screen blood when it happens right in front of the camera. Everything respects the Gore setting:
 * 0 Off (no blood, no dismemberment), 1 Reduced (blood, no dismemberment), 2 Full.
 */
UCLASS()
class ZOMBIESHOOTER_API UZSGoreSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static int32 GetGoreLevel();
	static bool BloodEnabled() { return GetGoreLevel() >= 1; }
	static bool DismemberEnabled() { return GetGoreLevel() >= 2; }

	/** Burst of flesh chunks. */
	void SpawnGibs(const FVector& Location, const FVector& Direction, int32 Count, float Scale = 1.f);
	/** The real body part flies off: a copy of the zombie's mesh showing only Bone and what hangs off it (head, forearm,
	 *  leg...), everything else collapsed to nothing, so the cut reads as a clean cut. */
	void SpawnSeveredPart(USkeletalMeshComponent* Source, FName Bone, const FVector& Direction);

	/** Blood pumping out of a bone stump for a few seconds. */
	void AddStump(USkeletalMeshComponent* Mesh, FName Bone, float Duration = 2.5f);
	/** Screen blood if Location is close to the player's camera. */
	void SplatterNearCamera(const FVector& Location, float Radius, float Amount);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	int32 MaxGibs = 48;

private:
	struct FStump
	{
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		FName Bone;
		float TimeLeft = 0.f;
		float NextSpurt = 0.f;
	};
	TArray<FStump> Stumps;

	struct FGib
	{
		TWeakObjectPtr<UStaticMeshComponent> Comp;
		float Life = 0.f;
		float DripTimer = 0.f;
		bool bSplatted = false;
		TWeakObjectPtr<class UPoseableMeshComponent> Part;   // severed body part riding on this physics carrier
	};
	TArray<FGib> Gibs;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Host;
};
