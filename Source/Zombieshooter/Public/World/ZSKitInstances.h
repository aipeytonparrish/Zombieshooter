#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZSKitInstances.generated.h"

class UStaticMesh;
class UInstancedStaticMeshComponent;

/** One mesh and every place it goes. */
USTRUCT(BlueprintType)
struct FZSKitBatch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	TArray<FTransform> Instances;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	bool bCollide = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	bool bCastShadow = true;
};

/**
 * A level section built from modular kit pieces (walls, floors, ceilings), drawn as one instanced mesh per piece:
 * thousands of pieces for a handful of draw calls (the kit is Nanite, so plain ISMs: Nanite does the culling). Filled in by the map scripts (create_nyc_map.py),
 * rebuilt on construction and at BeginPlay.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSKitInstances : public AActor
{
	GENERATED_BODY()

public:
	AZSKitInstances();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Kit")
	TArray<FZSKitBatch> Batches;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void Rebuild();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Components;
};
