#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ZSInteractable.h"
#include "ZSBarricade.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class USoundBase;

/**
 * A boarded window in the classic round-based style. Zombies spawned outside walk up to it, tear the boards off one by
 * one, then climb through; the player repairs it board by board (Interact) for a little run cash.
 *
 * Place it in the window opening with its forward (+X) pointing OUT of the building. The player can never pass
 * through (a blocker fills the opening), bullets can (shoot them through the gaps).
 */
UCLASS()
class ZOMBIESHOOTER_API AZSBarricade : public AActor, public IZSInteractable
{
	GENERATED_BODY()

public:
	AZSBarricade();

	/** Height of the window sill above the actor (the actor sits at floor level). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Barricade")
	float SillHeight = 90.f;

	/** Opening size (width, height) in cm, measured from the sill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Barricade")
	FVector2D OpeningSize = FVector2D(180.f, 150.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Barricade", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxBoards = 6;

	/** Run cash for each board put back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Barricade")
	int32 RepairReward = 10;

	/** How far outside / inside the opening zombies stand before / after climbing through (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Barricade")
	float StandOff = 85.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Barricade")
	TObjectPtr<UMaterialInterface> BoardMaterial;

	int32 GetBoards() const { return Boards; }
	bool HasBoards() const { return Boards > 0; }

	/** World points: where a zombie waits outside, the sill it climbs over, where it lands inside. */
	FVector GetOutsidePoint() const;
	FVector GetSillPoint() const;
	FVector GetInsidePoint() const;

	/** A zombie rips the next board off (flung inward). */
	void TearBoard(AActor* Zombie);

	// IZSInteractable
	virtual FText GetInteractPrompt() const override;
	virtual bool CanInteract(const AZSPlayerCharacter* Player) const override;
	virtual void Interact(AZSPlayerCharacter* Player) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void Build();
	void RefreshBoards();

	UPROPERTY(VisibleAnywhere, Category = "ZS|Barricade")
	TObjectPtr<UBoxComponent> Blocker;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BoardMeshes;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> BreakSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> RepairSound;

	int32 Boards = 6;
	double LastRepairTime = -10.0;
};
