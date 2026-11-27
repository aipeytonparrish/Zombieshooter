#include "World/ZSTaskMarker.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AZSTaskMarker::AZSTaskMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(RootComponent);
	Ring->SetStaticMesh(Cyl.Object);
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCastShadow(false);
	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	Beam->SetupAttachment(RootComponent);
	Beam->SetStaticMesh(Cyl.Object);
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetCastShadow(false);
	Beam->SetRelativeLocation(FVector(0.f, 0.f, 400.f));
	Beam->SetRelativeScale3D(FVector(0.12f, 0.12f, 8.f));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(80.f);
	Light->SetAttenuationRadius(900.f);
	Light->SetCastShadows(false);
}

void AZSTaskMarker::Setup(float Radius, const FLinearColor& Color, bool bBeam)
{
	Beam->SetVisibility(bBeam);
	Ring->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, 0.04f));
	Ring->SetRelativeLocation(FVector(0.f, 0.f, 3.f));
	if (UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		for (UStaticMeshComponent* C : { Ring.Get(), Beam.Get() })
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Shape, C);
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			C->SetMaterial(0, MID);
		}
	}
	Light->SetLightColor(Color);
}

void AZSTaskMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	Light->SetIntensity(60.f + 40.f * FMath::Sin(Age * 4.f));
	Beam->AddRelativeRotation(FRotator(0.f, 90.f * DeltaSeconds, 0.f));
}
