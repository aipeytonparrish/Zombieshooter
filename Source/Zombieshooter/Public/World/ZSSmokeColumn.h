#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZSSmokeColumn.generated.h"

/**
 * A column of smoke rising from a fire (and embers drifting up from its base): big dark billows that climb, spread and
 * drift with the wind. Drawn by the shared particle pool (UZSBloodSubsystem), so a skyline full of them stays cheap.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSSmokeColumn : public AActor
{
	GENERATED_BODY()

public:
	AZSSmokeColumn();

	/** Billow size at the base / at the top (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Smoke")
	FVector2D Size = FVector2D(250.f, 1400.f);

	/** Rise speed (cm/s) and how long a billow lives (s): height = speed x life. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Smoke")
	float RiseSpeed = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Smoke")
	float Life = 14.f;

	/** Billows per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Smoke")
	float Rate = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Smoke")
	FLinearColor Color = FLinearColor(0.035f, 0.033f, 0.03f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Smoke")
	float Opacity = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Smoke")
	FVector Wind = FVector(90.f, 40.f, 0.f);

	/** Embers per second rising from the base (0 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Smoke")
	float EmberRate = 6.f;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	float SmokeAcc = 0.f;
	float EmberAcc = 0.f;
};
