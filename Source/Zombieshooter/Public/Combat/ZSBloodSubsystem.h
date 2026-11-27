#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZSBloodSubsystem.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UDecalComponent;
class USoundBase;

/** What a bullet hit, for impact effects and sounds (guessed from the surface's material / mesh name). */
enum class EZSSurface : uint8
{
	Concrete,
	Metal,
	Wood,
	Glass,
	Dirt,
	Flesh,
};

/**
 * Blood and bullet impacts for the whole world, all pooled:
 *  - particles: billboarded instanced quads (blood mist, droplets, dust, debris, sparks), one draw call per kind;
 *  - decals: textured blood splats / pools that grow in, and bullet holes (oldest recycled past the caps);
 *  - impact sounds per surface (RealWeaponSoundFX bullet cues), throttled so a shotgun blast isn't 9 sounds.
 * Materials and textures are our own (Tools/Import/create_gore_fx.py).
 */
UCLASS()
class ZOMBIESHOOTER_API UZSBloodSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** A bullet / blade hit flesh: exit spray, mist, droplets, a splat on the wall / floor behind. Direction = travel. */
	void SpawnHitBlood(const FVector& Location, const FVector& Direction, float Intensity = 1.f);

	/** Pool of blood under a body (on death). */
	void SpawnPool(const FVector& Location);

	/** A bullet hit world geometry: hole decal, dust / sparks / splinters, impact sound. */
	void SpawnImpact(const FHitResult& Hit, const FVector& Direction, bool bPlaySound = true);

	/** A few drops falling from a point (stumps, gibs in flight). */
	void SpawnDrip(const FVector& Location, const FVector& Velocity, int32 Count = 2);

	/** A puff of dust (slides, landings). */
	void SpawnDust(const FVector& Location, const FVector& Velocity, float Scale = 1.f);

	/** Splat decal on a surface (normal points out of it). */
	void SpawnSplat(const FVector& Location, const FVector& Normal, float Size);

	static EZSSurface SurfaceOf(const FHitResult& Hit);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	int32 MaxParticles = 900;
	int32 MaxBloodDecals = 96;
	int32 MaxHoleDecals = 160;

private:
	enum class EKind : uint8 { Puff, Drop, Spark };

	struct FParticle
	{
		FVector Location;
		FVector Velocity;
		FLinearColor Color;
		float Life = 0.f;
		float MaxLife = 0.5f;
		float Size = 5.f;
		float EndSize = 5.f;
		float Gravity = 0.f;      // fraction of world gravity
		float Drag = 0.f;         // velocity damping per second
		float Opacity = 1.f;
		float Stretch = 0.f;      // length scale from speed (drops / sparks)
		EKind Kind = EKind::Puff;
	};

	struct FDecalSlot
	{
		TObjectPtr<UDecalComponent> Decal;
		TObjectPtr<class UMaterialInstanceDynamic> MID;
		float Grow = 1.f;       // 0..1 growth in progress
		float TargetSize = 10.f;
		float GrowRate = 2.5f;  // per second
	};

	void EnsureResources();
	UInstancedStaticMeshComponent* MakeISM(const TCHAR* Name, UMaterialInterface* Material);
	FParticle* AddParticle(EKind Kind);
	FDecalSlot* NextSlot(TArray<FDecalSlot>& Pool, int32& Next, int32 Max, UMaterialInterface* Material);
	void UpdateISM(UInstancedStaticMeshComponent* ISM, EKind Kind, const FVector& CamLoc);
	void PlayImpactSound(EZSSurface Surface, const FVector& Location);

	UPROPERTY(Transient) TObjectPtr<AActor> Host;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> PuffISM;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> DropISM;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> SparkISM;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> SplatMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> PoolMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> HoleMaterial;
	UPROPERTY(Transient) TMap<uint8, TObjectPtr<USoundBase>> ImpactSounds;
	UPROPERTY(Transient) TObjectPtr<USoundBase> FleshSound;

	TArray<FParticle> Particles;
	TArray<FDecalSlot> BloodDecals;
	TArray<FDecalSlot> HoleDecals;
	int32 NextBlood = 0;
	int32 NextHole = 0;
	double LastImpactSoundTime = -1.0;
	double LastFleshSoundTime = -1.0;
	bool bTriedResources = false;
};
