#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ZSInteractable.h"
#include "ZSBuyDoor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class USoundBase;
class UMaterialInterface;

/**
 * A boarded-up doorway the player buys open with run cash (classic round-based survival). Opening it knocks the boards
 * out, removes the blocker and opens its Zone, so spawn points in that zone (AZSSpawnPoint::Zone) start being used.
 * The blocker doesn't affect the NavMesh, so zombies path through the doorway as soon as it is open.
 * Place it centred in the opening, X pointing through the doorway; size it with OpeningSize.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSBuyDoor : public AActor, public IZSInteractable
{
	GENERATED_BODY()

public:
	AZSBuyDoor();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Door")
	int32 Cost = 500;

	/** Zone opened when bought (spawn points with this Zone start spawning). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Door")
	FName Zone = TEXT("Street");

	/** Shown in the prompt: "OPEN <Label>  $500". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Door")
	FText Label;

	/** Width (Y) and height (Z) of the doorway in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Door")
	FVector2D OpeningSize = FVector2D(140.f, 230.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Door")
	TObjectPtr<USoundBase> OpenSound;

	/** Material for the boards (e.g. rusty steel from the level's kit); dark wood placeholder colour if empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Door")
	TObjectPtr<UMaterialInterface> BoardMaterial;

	bool IsOpen() const { return bOpen; }

	virtual FText GetInteractPrompt() const override;
	virtual bool CanInteract(const AZSPlayerCharacter* Player) const override { return !bOpen; }
	virtual void Interact(AZSPlayerCharacter* Player) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void Build();
	void Open();
	int32 GetRunCash() const;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Door")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Door")
	TObjectPtr<UBoxComponent> Blocker;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Door")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Boards;

	bool bOpen = false;
};
