#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZSTaskMarker.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/** Glowing ring + light beam marking a timed-task zone or destination. */
UCLASS()
class ZOMBIESHOOTER_API AZSTaskMarker : public AActor
{
	GENERATED_BODY()

public:
	AZSTaskMarker();

	void Setup(float Radius, const FLinearColor& Color, bool bBeam = true);
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Ring;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Beam;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
	float Age = 0.f;
};
