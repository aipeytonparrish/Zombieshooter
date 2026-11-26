#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZSMovementCourse.generated.h"

class UInstancedStaticMeshComponent;
class UTextRenderComponent;

/**
 * Self-building graybox course for testing the movement kit. Drop one into any level (it builds
 * itself in the editor via OnConstruction) and press Play. All geometry uses the engine cube with
 * the world-grid material, so distances are readable (1 grid square = 1 m).
 *
 * Lanes (left to right, all running along +X from the actor):
 *   Dash gap | Mantle heights | Slide tunnel + slide ramp | Wall-run gap | Wall-run corridor | Double-jump tower | Shooting range
 */
UCLASS()
class ZOMBIESHOOTER_API AZSMovementCourse : public AActor
{
	GENERATED_BODY()

public:
	AZSMovementCourse();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Build a 100 m x 70 m floor under the course (turn off if the level already has a floor). */
	UPROPERTY(EditAnywhere, Category = "ZS|Course")
	bool bBuildFloor = true;

	UPROPERTY(EditAnywhere, Category = "ZS|Course")
	TObjectPtr<UMaterialInterface> BlockMaterial;

	/** Spawn the shooting-range target dummies (lane 7) when play starts. */
	UPROPERTY(EditAnywhere, Category = "ZS|Course")
	bool bSpawnTargets = true;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Course")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Course")
	TObjectPtr<UInstancedStaticMeshComponent> Blocks;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Course")
	TArray<TObjectPtr<UTextRenderComponent>> Labels;

private:
	static constexpr int32 NumLabels = 7;

	void Rebuild();

	/** Spawn points + ammo crates for survival when the level has none (quick-test map). */
	void SpawnSurvivalProps();

	/** Axis-aligned box from min corner to max corner (local space, cm). */
	void AddBox(const FVector& Min, const FVector& Max);

	/** Sloped slab whose top surface runs from (StartX, ground) to (StartX + Length, Rise). Negative Rise = downhill. */
	void AddRamp(float StartX, float CenterY, float Length, float Width, float BaseZ, float Rise);

	void SetLabel(int32 Index, const FVector& Location, const FString& Text);
};
