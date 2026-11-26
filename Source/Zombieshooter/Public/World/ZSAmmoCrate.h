#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ZSInteractable.h"
#include "ZSAmmoCrate.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UBoxComponent;
class USoundBase;

/**
 * Unlimited ammo crate: Interact refills the reserve of every carried weapon, any number of times, no cooldown.
 * Placement (not scarcity) is what makes players move around the map. Built from the owner's ammo-box meshes;
 * grey boxes if they haven't been imported.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSAmmoCrate : public AActor, public IZSInteractable
{
	GENERATED_BODY()

public:
	AZSAmmoCrate();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Crate")
	TObjectPtr<USoundBase> RefillSound;

	virtual FText GetInteractPrompt() const override;
	virtual void Interact(AZSPlayerCharacter* Player) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	void ApplyVisuals();

	UPROPERTY(VisibleAnywhere, Category = "ZS|Crate")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Crate")
	TObjectPtr<UStaticMeshComponent> BoxA;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Crate")
	TObjectPtr<UStaticMeshComponent> BoxB;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Crate")
	TObjectPtr<UPointLightComponent> Light;

	/** Blocks movement and the interact trace (independent of the art's collision). */
	UPROPERTY(VisibleAnywhere, Category = "ZS|Crate")
	TObjectPtr<UBoxComponent> Collision;
};
