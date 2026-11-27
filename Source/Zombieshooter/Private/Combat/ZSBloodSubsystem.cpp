#include "Combat/ZSBloodSubsystem.h"
#include "Combat/ZSGoreSubsystem.h"
#include "Weapons/ZSWeaponTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Zombieshooter.h"

namespace
{
	const TCHAR* DecalMaterialPath = TEXT("/Game/ZS/Materials/M_ZS_BloodDecal.M_ZS_BloodDecal");
	const FLinearColor BloodColor(0.09f, 0.0f, 0.0f);
}

bool UZSBloodSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId UZSBloodSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZSBloodSubsystem, STATGROUP_Tickables);
}

void UZSBloodSubsystem::EnsureResources()
{
	if (bTriedResources)
	{
		return;
	}
	bTriedResources = true;
	UWorld* World = GetWorld();

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Host)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Host, TEXT("Root"));
	Host->SetRootComponent(Root);
	Root->RegisterComponent();

	DropletMesh = NewObject<UInstancedStaticMeshComponent>(Host, TEXT("BloodDroplets"));
	DropletMesh->SetupAttachment(Root);
	DropletMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	DropletMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DropletMesh->SetCastShadow(false);
	DropletMesh->SetMobility(EComponentMobility::Movable);
	if (UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Shape, DropletMesh);
		MID->SetVectorParameterValue(TEXT("Color"), BloodColor);
		DropletMesh->SetMaterial(0, MID);
	}
	DropletMesh->RegisterComponent();

	DecalMaterial = LoadObject<UMaterialInterface>(nullptr, DecalMaterialPath);
	if (!DecalMaterial)
	{
		UE_LOG(LogZS, Warning, TEXT("Blood decal material %s missing - run Tools/Import/run_imports.ps1."), DecalMaterialPath);
	}
}

void UZSBloodSubsystem::SpawnHitBlood(const FVector& Location, const FVector& Direction, float Intensity)
{
	if (!UZSGoreSubsystem::BloodEnabled())
	{
		return;
	}
	MaxDecals = UZSGoreSubsystem::DismemberEnabled() ? 256 : 48;
	EnsureResources();
	UWorld* World = GetWorld();
	const FVector Dir = Direction.GetSafeNormal();

	// Droplets: mostly out of the exit side, a few back toward the shooter.
	const int32 Count = FMath::RoundToInt(10.f * Intensity);
	for (int32 i = 0; i < Count && Droplets.Num() < MaxDroplets; ++i)
	{
		FDroplet& D = Droplets.AddDefaulted_GetRef();
		const bool bBack = i % 4 == 0;
		const FVector Cone = FMath::VRandCone(bBack ? -Dir : Dir, FMath::DegreesToRadians(bBack ? 35.f : 25.f));
		D.Location = Location;
		D.Velocity = Cone * FMath::FRandRange(150.f, 480.f) + FVector(0.f, 0.f, FMath::FRandRange(40.f, 160.f));
		D.MaxLife = FMath::FRandRange(0.35f, 0.75f);
		D.Size = FMath::FRandRange(0.6f, 1.6f);
	}

	// Splat on whatever is behind the target (wall, floor) within 3 m.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSBlood), false);
	Params.bReturnPhysicalMaterial = false;
	const FVector Start = Location + Dir * 40.f;
	const FVector End = Start + (Dir + FVector(0.f, 0.f, -0.35f)).GetSafeNormal() * 300.f;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && !Hit.GetActor()->IsA<APawn>())
	{
		SpawnDecal(Hit.ImpactPoint, Hit.ImpactNormal, FMath::FRandRange(18.f, 34.f) * Intensity);
	}
}

void UZSBloodSubsystem::SpawnPool(const FVector& Location)
{
	if (!UZSGoreSubsystem::BloodEnabled())
	{
		return;
	}
	EnsureResources();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSBloodPool), false);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 50.f), Location - FVector(0.f, 0.f, 200.f), ECC_Visibility, Params))
	{
		SpawnDecal(Hit.ImpactPoint, Hit.ImpactNormal, FMath::FRandRange(55.f, 85.f));
	}
}

void UZSBloodSubsystem::SpawnDecal(const FVector& Location, const FVector& Normal, float Size)
{
	if (!DecalMaterial || !Host)
	{
		return;
	}

	// Decals project along their X axis: point it into the surface, random spin around the normal.
	FRotator Rot = (-Normal).Rotation();
	Rot.Roll = FMath::FRandRange(0.f, 360.f);
	const FVector DecalSize(12.f, Size, Size);

	UDecalComponent* Decal = nullptr;
	if (Decals.Num() < MaxDecals)
	{
		Decal = NewObject<UDecalComponent>(Host);
		Decal->SetupAttachment(Host->GetRootComponent());
		Decal->SetDecalMaterial(DecalMaterial);
		Decal->SetFadeScreenSize(0.0005f);
		Decal->RegisterComponent();
		Decals.Add(Decal);
	}
	else
	{
		Decal = Decals[NextDecal];
		NextDecal = (NextDecal + 1) % Decals.Num();
	}
	Decal->DecalSize = DecalSize;
	Decal->SetWorldLocationAndRotation(Location, Rot);
	Decal->MarkRenderStateDirty();
}

void UZSBloodSubsystem::Tick(float DeltaTime)
{
	if (!DropletMesh)
	{
		return;
	}

	const float Gravity = -980.f;
	for (int32 i = Droplets.Num() - 1; i >= 0; --i)
	{
		FDroplet& D = Droplets[i];
		D.Life += DeltaTime;
		D.Velocity.Z += Gravity * DeltaTime;
		D.Velocity *= FMath::Max(0.f, 1.f - 1.5f * DeltaTime);
		D.Location += D.Velocity * DeltaTime;
		if (D.Life >= D.MaxLife)
		{
			Droplets.RemoveAtSwap(i, EAllowShrinking::No);
		}
	}

	// Rebuild instance transforms (cheap: tiny arrays, one component).
	TArray<FTransform> Transforms;
	Transforms.Reserve(Droplets.Num());
	for (const FDroplet& D : Droplets)
	{
		const float Shrink = 1.f - D.Life / D.MaxLife;
		const float S = 0.018f * D.Size * (0.4f + 0.6f * Shrink);
		const FVector Stretch = D.Velocity.GetSafeNormal();
		const FRotator R = Stretch.IsNearlyZero() ? FRotator::ZeroRotator : Stretch.Rotation();
		Transforms.Emplace(R, D.Location, FVector(S * 2.2f, S, S));
	}

	if (DropletMesh->GetInstanceCount() != Transforms.Num())
	{
		DropletMesh->ClearInstances();
		DropletMesh->AddInstances(Transforms, false, true);
	}
	else if (Transforms.Num() > 0)
	{
		DropletMesh->BatchUpdateInstancesTransforms(0, Transforms, true, true);
	}
}
