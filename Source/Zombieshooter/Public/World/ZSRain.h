#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZSRain.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Cheap rain: thin instanced streaks (M_ZS_Rain) simulated in world space inside a box around the local player's
 * camera, wrapping around it as it moves (so the rain doesn't slide along with you). Drop one in a level and tune it.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSRain : public AActor
{
	GENERATED_BODY()

public:
	AZSRain();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Number of streaks in the box. */
	UPROPERTY(EditAnywhere, Category = "ZS|Rain", meta = (ClampMin = "0", ClampMax = "20000"))
	int32 DropCount = 3500;

	/** Half size of the rain box around the camera. */
	UPROPERTY(EditAnywhere, Category = "ZS|Rain")
	FVector BoxExtent = FVector(1000.f, 1000.f, 700.f);

	UPROPERTY(EditAnywhere, Category = "ZS|Rain")
	float FallSpeed = 1150.f;

	/** Streak length / width in cm. */
	UPROPERTY(EditAnywhere, Category = "ZS|Rain")
	FVector2D StreakSize = FVector2D(90.f, 2.2f);

	UPROPERTY(EditAnywhere, Category = "ZS|Rain")
	float Opacity = 0.3f;

	UPROPERTY(EditAnywhere, Category = "ZS|Rain")
	FLinearColor Color = FLinearColor(0.6f, 0.64f, 0.7f, 1.f);

	/** Sideways drift per cm fallen (wind). */
	UPROPERTY(EditAnywhere, Category = "ZS|Rain")
	float Wind = 0.12f;

private:
	FTransform DropTransform(int32 Index) const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Drops;

	TArray<FVector> Positions;
	TArray<float> Yaws;
};
