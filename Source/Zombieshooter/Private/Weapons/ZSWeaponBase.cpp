#include "Weapons/ZSWeaponBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Zombieshooter.h"

namespace ZSWeaponPaths
{
	static const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
	static const TCHAR* Cylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	static const TCHAR* ShapeMaterial = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
}

AZSWeaponBase::AZSWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);

	WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
	RootComponent = WeaponRoot;

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(WeaponRoot);
	MuzzleLight->SetIntensityUnits(ELightUnits::Candelas);
	MuzzleLight->SetIntensity(0.f);
	MuzzleLight->SetAttenuationRadius(450.f);
	MuzzleLight->SetLightColor(FLinearColor(1.f, 0.62f, 0.25f));
	MuzzleLight->SetCastShadows(false);

	Stats.FireModes = { EZSFireMode::Auto, EZSFireMode::Semi };
}

// ---- Part helpers ---------------------------------------------------------------------------

FZSWeaponPart AZSWeaponBase::MakeImportedPart(FName Name, const TCHAR* MeshPath, bool bMagazine)
{
	FZSWeaponPart Part;
	Part.Name = Name;
	Part.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
	Part.bIsMagazine = bMagazine;
	return Part;
}

FZSWeaponPart AZSWeaponBase::MakeBoxPart(FName Name, const FVector& Min, const FVector& Max, const FLinearColor& Color, bool bMagazine)
{
	FZSWeaponPart Part;
	Part.Name = Name;
	Part.PlaceholderShape = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(ZSWeaponPaths::Cube));
	Part.PlaceholderOffset = FTransform(FRotator::ZeroRotator, (Min + Max) * 0.5f, (Max - Min).GetAbs() / 100.f);
	Part.PlaceholderColor = Color;
	Part.bIsMagazine = bMagazine;
	return Part;
}

FZSWeaponPart AZSWeaponBase::MakeCylinderPart(FName Name, const FVector& Center, float Length, float Diameter, const FLinearColor& Color)
{
	FZSWeaponPart Part;
	Part.Name = Name;
	Part.PlaceholderShape = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(ZSWeaponPaths::Cylinder));
	// Engine cylinder is 100 cm tall along Z; pitch it to lie along X.
	Part.PlaceholderOffset = FTransform(FRotator(90.f, 0.f, 0.f), Center, FVector(Diameter, Diameter, Length) / 100.f);
	Part.PlaceholderColor = Color;
	return Part;
}

// ---- Lifecycle ------------------------------------------------------------------------------

void AZSWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	if (Stats.FireModes.Num() == 0)
	{
		Stats.FireModes.Add(EZSFireMode::Semi);
	}
	RefillAmmo();
	BuildVisuals();
}

void AZSWeaponBase::BuildVisuals()
{
	UMaterialInterface* ShapeMaterial = LoadObject<UMaterialInterface>(nullptr, ZSWeaponPaths::ShapeMaterial);
	int32 Missing = 0;

	for (const FZSWeaponPart& Part : Stats.Parts)
	{
		UStaticMesh* Mesh = Part.Mesh.IsNull() ? nullptr : Part.Mesh.LoadSynchronous();
		FTransform Offset = Part.Offset;
		bool bPlaceholder = false;

		if (!Mesh)
		{
			if (!Part.Mesh.IsNull())
			{
				++Missing;
			}
			Mesh = Part.PlaceholderShape.IsNull() ? nullptr : Part.PlaceholderShape.LoadSynchronous();
			Offset = Part.PlaceholderOffset;
			bPlaceholder = true;
		}
		if (!Mesh)
		{
			continue;
		}

		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, NAME_None);
		Comp->SetupAttachment(WeaponRoot);
		Comp->SetStaticMesh(Mesh);
		Comp->SetRelativeTransform(Offset);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetOnlyOwnerSee(true);
		Comp->SetCastShadow(false);
		Comp->bSelfShadowOnly = false;
		Comp->RegisterComponent();

		if (bPlaceholder && ShapeMaterial)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ShapeMaterial, Comp);
			MID->SetVectorParameterValue(TEXT("Color"), Part.PlaceholderColor);
			Comp->SetMaterial(0, MID);
		}

		PartComponents.Add(Comp);
		if (Part.bIsMagazine)
		{
			MagazineComponents.Add(Comp);
			MagazineBaseTransforms.Add(Offset);
		}
	}

	if (Missing > 0)
	{
		UE_LOG(LogZS, Warning, TEXT("%s: %d imported part mesh(es) missing - using placeholders. Run Tools/Import scripts (Docs/ThirdPartyAssets.md)."),
			*GetName(), Missing);
	}

	MuzzleLight->SetRelativeLocation(Stats.MuzzlePoint + FVector(25.f, 0.f, 0.f));
}

void AZSWeaponBase::SetWeaponVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
}

void AZSWeaponBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MuzzleFlashTime > 0.f)
	{
		MuzzleFlashTime = FMath::Max(0.f, MuzzleFlashTime - DeltaSeconds);
		const float Alpha = MuzzleFlashTime / 0.05f;
		MuzzleLight->SetIntensity(Alpha * 120.f);
	}
}

// ---- Ammo -----------------------------------------------------------------------------------

void AZSWeaponBase::RefillAmmo()
{
	AmmoInMag = Stats.MagSize;
	ReserveAmmo = Stats.MaxReserve;
}

bool AZSWeaponBase::ConsumeRound()
{
	if (AmmoInMag <= 0)
	{
		return false;
	}
	--AmmoInMag;
	return true;
}

bool AZSWeaponBase::CanReload() const
{
	const int32 Capacity = Stats.MagSize + ((Stats.bChamberRound && AmmoInMag > 0) ? 1 : 0);
	return ReserveAmmo > 0 && AmmoInMag < Capacity;
}

void AZSWeaponBase::CommitReload(bool bWasEmpty)
{
	const int32 Capacity = Stats.MagSize + ((Stats.bChamberRound && !bWasEmpty) ? 1 : 0);
	const int32 Needed = FMath::Max(0, Capacity - AmmoInMag);
	const int32 Taken = FMath::Min(Needed, ReserveAmmo);
	AmmoInMag += Taken;
	ReserveAmmo -= Taken;
}

EZSFireMode AZSWeaponBase::GetFireMode() const
{
	return Stats.FireModes.IsValidIndex(FireModeIndex) ? Stats.FireModes[FireModeIndex] : EZSFireMode::Semi;
}

void AZSWeaponBase::CycleFireMode()
{
	if (Stats.FireModes.Num() > 1)
	{
		FireModeIndex = (FireModeIndex + 1) % Stats.FireModes.Num();
	}
}

// ---- Visual helpers -------------------------------------------------------------------------

FTransform AZSWeaponBase::GetWeaponSpaceToWorld() const
{
	return WeaponRoot->GetComponentTransform();
}

FVector AZSWeaponBase::GetMuzzleWorldLocation() const
{
	return GetWeaponSpaceToWorld().TransformPosition(Stats.MuzzlePoint);
}

void AZSWeaponBase::PlayMuzzleFlash()
{
	MuzzleFlashTime = 0.05f;
	MuzzleLight->SetIntensity(120.f);
}

void AZSWeaponBase::SetMagazineOffset(const FVector& Offset, const FRotator& Rotation)
{
	for (int32 i = 0; i < MagazineComponents.Num(); ++i)
	{
		if (MagazineComponents[i] && MagazineBaseTransforms.IsValidIndex(i))
		{
			const FTransform Delta(Rotation, Offset);
			MagazineComponents[i]->SetRelativeTransform(MagazineBaseTransforms[i] * Delta);
		}
	}
}
