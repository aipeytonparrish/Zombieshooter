#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZSRain.generated.h"

class UInstancedStaticMeshComponent;
class USoundBase;

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

	/** World-space boxes with a roof (building interiors): no drops inside them. */
	UPROPERTY(EditAnywhere, Category = "ZS|Rain")
	TArray<FBox> ShelterBoxes;

	/** Storm: random lightning flashes (the level's directional and sky lights spike briefly). */
	UPROPERTY(EditAnywhere, Category = "ZS|Rain|Lightning")
	bool bLightning = true;
	UPROPERTY(EditAnywhere, Category = "ZS|Rain|Lightning", meta = (ForceUnits = "s"))
	FVector2D LightningInterval = FVector2D(14.f, 35.f);
	UPROPERTY(EditAnywhere, Category = "ZS|Rain|Lightning")
	float LightningBoost = 7.f;
	UPROPERTY(EditAnywhere, Category = "ZS|Rain|Lightning")
	TObjectPtr<USoundBase> ThunderSound;

private:
	void UpdateLightning(float DeltaSeconds);
	bool IsSheltered(const FVector& P) const;

	UPROPERTY(Transient)
	TObjectPtr<class UDirectionalLightComponent> Sun;
	UPROPERTY(Transient)
	TObjectPtr<class USkyLightComponent> Sky;
	float SunBase = 0.f;
	float SkyBase = 0.f;
	float NextFlash = 0.f;
	float FlashTime = -1.f;
	float ThunderDelay = -1.f;

	FTransform DropTransform(int32 Index) const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Drops;

	TArray<FVector> Positions;
	TArray<float> Yaws;
};
