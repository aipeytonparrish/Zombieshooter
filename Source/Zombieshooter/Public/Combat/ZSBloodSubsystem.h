#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZSBloodSubsystem.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UDecalComponent;

/**
 * Basic blood (Milestone 4): a burst of droplets on every flesh hit (one pooled instanced mesh for the whole
 * world, so hundreds of droplets cost one draw call) and blood splat decals on nearby walls / floor that stay
 * for the rest of the run (oldest recycled past MaxDecals). Full gore (dismemberment, Niagara spray, screen
 * blood) arrives in Milestone 11.
 */
UCLASS()
class ZOMBIESHOOTER_API UZSBloodSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Spray where a bullet hit flesh. Direction = bullet travel direction. Also splats the surface behind. */
	void SpawnHitBlood(const FVector& Location, const FVector& Direction, float Intensity = 1.f);

	/** Pool of blood under a body (on death). */
	void SpawnPool(const FVector& Location);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	int32 MaxDroplets = 400;
	int32 MaxDecals = 96;

private:
	struct FDroplet
	{
		FVector Location;
		FVector Velocity;
		float Life = 0.f;
		float MaxLife = 0.6f;
		float Size = 1.f;
	};

	void EnsureResources();
	void SpawnDecal(const FVector& Location, const FVector& Normal, float Size);

	UPROPERTY(Transient)
	TObjectPtr<AActor> Host;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> DropletMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DecalMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> Decals;

	TArray<FDroplet> Droplets;
	int32 NextDecal = 0;
	bool bTriedResources = false;
};
