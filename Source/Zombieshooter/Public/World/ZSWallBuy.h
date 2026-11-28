#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ZSInteractable.h"
#include "ZSWallBuy.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UTextRenderComponent;
class USoundBase;

/** A gun's look built from its class defaults (wall buys, the mystery box). */
namespace ZSWeaponDisplay
{
	void Build(AActor* Owner, USceneComponent* Parent, FName ItemId, TArray<TObjectPtr<USceneComponent>>& Out, const FTransform& Offset);
}

/**
 * A gun hanging on the wall (classic round-based survival): buy it with run cash, or buy ammo for it if you already
 * carry one. In-run guns are yours until the run ends; their kills still level the gun type up.
 * Place it against a wall with its forward (+X) pointing out into the room.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSWallBuy : public AActor, public IZSInteractable
{
	GENERATED_BODY()

public:
	AZSWallBuy();

	/** ZSWeaponCatalog id: PST_P9, AR_M4, AR_RD704... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|WallBuy")
	FName ItemId = TEXT("AR_M4");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|WallBuy")
	int32 Cost = 1000;

	/** Ammo refill when you already have this gun (default half the gun). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|WallBuy")
	int32 AmmoCost = -1;

	virtual FText GetInteractPrompt() const override;
	virtual void Interact(AZSPlayerCharacter* Player) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void BuildDisplay();
	int32 GetAmmoCost() const { return AmmoCost >= 0 ? AmmoCost : Cost / 2; }

	UPROPERTY(VisibleAnywhere, Category = "ZS|WallBuy")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, Category = "ZS|WallBuy")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(VisibleAnywhere, Category = "ZS|WallBuy")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> DisplayParts;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> BuySound;
};
