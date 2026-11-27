#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZSBunkerLayout.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Self-building placeholder bunker room: concrete floor, walls, ceiling, support pillars, crates and warm work lights.
 * Rebuilt from its settings in OnConstruction and BeginPlay, so the map only stores the actor. Swap for real art
 * later by deleting it and dressing the level by hand.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSBunkerLayout : public AActor
{
	GENERATED_BODY()

public:
	AZSBunkerLayout();

	/** Inner size of the room (cm). */
	UPROPERTY(EditAnywhere, Category = "ZS|Bunker") FVector RoomSize = FVector(2000.f, 1400.f, 420.f);
	UPROPERTY(EditAnywhere, Category = "ZS|Bunker") float WallThickness = 40.f;
	UPROPERTY(EditAnywhere, Category = "ZS|Bunker") FLinearColor ConcreteColor = FLinearColor(0.16f, 0.15f, 0.14f);
	UPROPERTY(EditAnywhere, Category = "ZS|Bunker") FLinearColor FloorColor = FLinearColor(0.09f, 0.09f, 0.085f);
	UPROPERTY(EditAnywhere, Category = "ZS|Bunker") float LightIntensity = 900.f;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void Build();
	UStaticMeshComponent* AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCollide = true);
	void AddLight(const FVector& Location, float Intensity, const FLinearColor& Color, float Radius);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> Built;
};
