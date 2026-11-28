#include "World/ZSSmokeColumn.h"
#include "Combat/ZSBloodSubsystem.h"
#include "Engine/World.h"

AZSSmokeColumn::AZSSmokeColumn()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AZSSmokeColumn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UZSBloodSubsystem* FX = GetWorld()->GetSubsystem<UZSBloodSubsystem>();
	if (!FX)
	{
		return;
	}
	const FVector Base = GetActorLocation();
	SmokeAcc += DeltaSeconds * Rate;
	while (SmokeAcc >= 1.f)
	{
		SmokeAcc -= 1.f;
		const FVector Jitter(FMath::FRandRange(-1.f, 1.f) * Size.X, FMath::FRandRange(-1.f, 1.f) * Size.X, 0.f);
		const FVector Vel = FVector(0.f, 0.f, RiseSpeed * FMath::FRandRange(0.8f, 1.2f)) + Wind * FMath::FRandRange(0.7f, 1.3f);
		FX->SpawnSmoke(Base + Jitter * 0.5f, Vel, Size.X, Size.Y * FMath::FRandRange(0.75f, 1.15f), Life * FMath::FRandRange(0.8f, 1.1f),
			Color * FMath::FRandRange(0.8f, 1.25f), Opacity);
	}
	EmberAcc += DeltaSeconds * EmberRate;
	while (EmberAcc >= 1.f)
	{
		EmberAcc -= 1.f;
		FX->SpawnEmber(Base + FVector(FMath::FRandRange(-1.f, 1.f) * Size.X * 0.6f, FMath::FRandRange(-1.f, 1.f) * Size.X * 0.6f, 0.f),
			FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(150.f, 380.f)) + Wind * 0.8f);
	}
}
