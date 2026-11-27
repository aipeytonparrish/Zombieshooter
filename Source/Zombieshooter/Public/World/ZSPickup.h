#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZSPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EZSPickupType : uint8
{
	Health,
	Armor,
	// Power-ups (Milestone 11)
	Overkill,
	OneShot,
	ScorchedEarth,
	Payday
};

/**
 * Floating, spinning pickup dropped by zombies (or placed). Walk through it to collect. It isn't taken when it
 * would do nothing (full health / full armor). Blinks for its last seconds, then disappears.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSPickup : public AActor
{
	GENERATED_BODY()

public:
	AZSPickup();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Pickup", meta = (ExposeOnSpawn = true))
	EZSPickupType Type = EZSPickupType::Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Pickup")
	float Amount = 50.f;

	/** Seconds before it vanishes (<= 0 = forever, for placed pickups). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Pickup")
	float Lifetime = 30.f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Pickup")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Pickup")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Pickup")
	TObjectPtr<UTextRenderComponent> Label;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	bool TryApply(AActor* Other);

	float Age = 0.f;
	float BaseZ = 0.f;
};
