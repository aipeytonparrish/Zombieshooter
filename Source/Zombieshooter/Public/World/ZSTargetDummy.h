#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZSTargetDummy.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UZSHealthComponent;
class UMaterialInstanceDynamic;

/**
 * Shooting-range target: a humanoid-sized body + head (tagged "Head" for headshots), a health readout,
 * hit flash, falls over when killed and stands back up after a few seconds. Optionally strafes side to side
 * for testing aim assist tracking. Registers itself with aim assist.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSTargetDummy : public AActor
{
	GENERATED_BODY()

public:
	AZSTargetDummy();

	/** Strafe left/right (along the actor's Y axis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Target")
	bool bMoving = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Target", meta = (EditCondition = "bMoving"))
	float MoveDistance = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Target", meta = (EditCondition = "bMoving"))
	float MoveSpeed = 250.f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Target")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Target")
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Target")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Target")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Target")
	TObjectPtr<UTextRenderComponent> Readout;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Target")
	TObjectPtr<UZSHealthComponent> Health;

private:
	UFUNCTION()
	void HandleDamaged(UZSHealthComponent* InHealth, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	void UpdateReadout();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeadMID;

	FVector StartLocation = FVector::ZeroVector;
	float MovePhase = 0.f;
	float FlashTime = 0.f;
	float FallAlpha = 0.f;
	float LastDamage = 0.f;
};
