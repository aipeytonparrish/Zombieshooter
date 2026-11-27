#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZSSpitProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UProjectileMovementComponent;

/** Spitter's acid glob: lobbed in an arc, splashes on impact and leaves an acid pool that burns the player. */
UCLASS()
class ZOMBIESHOOTER_API AZSSpitProjectile : public AActor
{
	GENERATED_BODY()

public:
	AZSSpitProjectile();

	float ImpactDamage = 12.f;
	float PoolDamagePerSecond = 10.f;
	float PoolLifetime = 5.f;
	float PoolRadius = 130.f;

	/** Velocity needed to land at Target from here (high-ish arc). */
	static FVector AimVelocity(const FVector& From, const FVector& Target, float Speed);
	void Launch(const FVector& Velocity);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Glob;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
	bool bHit = false;
};

/** Burning acid on the ground. */
UCLASS()
class ZOMBIESHOOTER_API AZSAcidPool : public AActor
{
	GENERATED_BODY()

public:
	AZSAcidPool();
	void Setup(float Radius, float DamagePerSecond, float Lifetime);
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Pool;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
	float Radius = 130.f;
	float DPS = 10.f;
	float Life = 5.f;
	float Age = 0.f;
	float DamageTick = 0.f;
};
