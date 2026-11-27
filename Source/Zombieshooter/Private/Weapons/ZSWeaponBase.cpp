#include "Weapons/ZSWeaponBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Weapons/ZSFPAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Weapons/ZSWeaponCatalog.h"
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

	DriverMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DriverMesh"));
	DriverMesh->SetupAttachment(WeaponRoot);
	DriverMesh->SetHiddenInGame(true);
	DriverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DriverMesh->SetCastShadow(false);
	DriverMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(DriverMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCastShadow(false);
	WeaponMesh->SetOnlyOwnerSee(true);
	WeaponMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	WeaponMesh->bUseAttachParentBound = false;
	WeaponMesh->SetBoundsScale(3.f);

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
	for (const FName Id : Attachments)
	{
		if (const ZSWeaponCatalog::FAttachment* A = ZSWeaponCatalog::FindAttachment(Id); A && A->Apply)
		{
			A->Apply(Stats);
		}
	}
	if (Stats.FireModes.Num() == 0)
	{
		Stats.FireModes.Add(EZSFireMode::Semi);
	}
	RefillAmmo();
	BuildVisuals();
}

void AZSWeaponBase::BuildVisuals()
{
	if (TryBuildAnimated())
	{
		MuzzleLight->AttachToComponent(WeaponMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			WeaponMesh->DoesSocketExist(AnimSet.MuzzleSocket) ? AnimSet.MuzzleSocket : NAME_None);
		if (!WeaponMesh->DoesSocketExist(AnimSet.MuzzleSocket))
		{
			MuzzleLight->SetRelativeLocation(AnimSet.MuzzleOffset);
		}
		return;
	}
	WeaponMesh->SetVisibility(false);

	UMaterialInterface* ShapeMaterial = LoadObject<UMaterialInterface>(nullptr, ZSWeaponPaths::ShapeMaterial);
	int32 Missing = 0;

	for (const FZSWeaponPart& Part : Stats.Parts)
	{
		if (!Part.RequiredAttachment.IsNone() && !Attachments.Contains(Part.RequiredAttachment))
		{
			continue;
		}
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
	if (bAnimated)
	{
		return WeaponMesh->DoesSocketExist(AnimSet.MuzzleSocket) ? WeaponMesh->GetSocketLocation(AnimSet.MuzzleSocket)
			: WeaponMesh->GetComponentTransform().TransformPosition(AnimSet.MuzzleOffset);
	}
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

// ---- Animated rig ---------------------------------------------------------------------------

namespace
{
	template <typename T>
	T* LoadKept(const TSoftObjectPtr<T>& Ptr, TArray<TObjectPtr<UObject>>& Keep)
	{
		if (Ptr.IsNull())
		{
			return nullptr;
		}
		T* Obj = Ptr.LoadSynchronous();
		if (Obj)
		{
			Keep.Add(Obj);
		}
		return Obj;
	}
}

bool AZSWeaponBase::TryBuildAnimated()
{
	FZSWeaponAnimSet& A = AnimSet;
	USkeletalMesh* Driver = LoadKept(A.DriverMesh, KeepAlive);
	USkeletalMesh* Gun = LoadKept(A.WeaponMesh, KeepAlive);
	UAnimSequence* Idle = LoadKept(A.Idle, KeepAlive);
	if (!Driver || !Gun || !Idle)
	{
		if (!A.DriverMesh.IsNull())
		{
			UE_LOG(LogZS, Warning, TEXT("%s: animation pack assets missing (%s) - using code-posed arms and placeholder parts."),
				*GetName(), *A.DriverMesh.ToString());
		}
		return false;
	}

	Loaded.Fire = LoadKept(A.Fire, KeepAlive);
	Loaded.FireAimed = LoadKept(A.FireAimed, KeepAlive);
	Loaded.Reload = LoadKept(A.Reload, KeepAlive);
	Loaded.ReloadEmpty = LoadKept(A.ReloadEmpty, KeepAlive);
	Loaded.Equip = LoadKept(A.Equip, KeepAlive);
	Loaded.Holster = LoadKept(A.Holster, KeepAlive);
	Loaded.WeaponFire = LoadKept(A.WeaponFire, KeepAlive);
	Loaded.WeaponReload = LoadKept(A.WeaponReload, KeepAlive);
	Loaded.WeaponReloadEmpty = LoadKept(A.WeaponReloadEmpty, KeepAlive);
	Loaded.ReloadSoundMontage = LoadKept(A.ReloadSoundMontage, KeepAlive);
	Loaded.ReloadEmptySoundMontage = LoadKept(A.ReloadEmptySoundMontage, KeepAlive);
	for (const TSoftObjectPtr<USoundBase>& S : A.FireSounds)
	{
		if (USoundBase* Sound = LoadKept(S, KeepAlive))
		{
			Loaded.FireSounds.Add(Sound);
		}
	}
	Loaded.FireTail = LoadKept(A.FireTail, KeepAlive);
	Loaded.DryFire = LoadKept(A.DryFire, KeepAlive);
	Loaded.EquipSound = LoadKept(A.EquipSound, KeepAlive);
	Loaded.MuzzleFlash = LoadKept(A.MuzzleFlash, KeepAlive);
	auto LoadTimed = [this](const TArray<FZSTimedSound>& In, TArray<FZSActionSound>& Out)
	{
		for (const FZSTimedSound& T : In)
		{
			if (USoundBase* Sound = LoadKept(T.Sound, KeepAlive))
			{
				FZSActionSound& S = Out.AddDefaulted_GetRef();
				S.Fraction = T.Fraction;
				S.Sound = Sound;
			}
		}
	};
	LoadTimed(A.ReloadSounds, Loaded.ReloadSounds);
	LoadTimed(A.ReloadEmptySounds, Loaded.ReloadEmptySounds);

	DriverMesh->SetSkeletalMeshAsset(Driver);
	DriverMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	DriverMesh->SetAnimInstanceClass(UZSFPAnimInstance::StaticClass());
	if (UZSFPAnimInstance* Arms = GetArmsAnim())
	{
		Arms->SetLoops(Idle, LoadKept(A.Walk, KeepAlive), LoadKept(A.Sprint, KeepAlive), LoadKept(A.AimIdle, KeepAlive), LoadKept(A.AimWalk, KeepAlive));
	}

	WeaponMesh->SetSkeletalMeshAsset(Gun);
	WeaponMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	WeaponMesh->SetAnimInstanceClass(UZSFPAnimInstance::StaticClass());
	if (UZSFPAnimInstance* GunAnim = GetWeaponAnim())
	{
		GunAnim->SetLoops(LoadKept(A.WeaponIdle, KeepAlive), nullptr, nullptr, nullptr, nullptr);
	}
	WeaponMesh->AttachToComponent(DriverMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, A.WeaponAttachBone);
	WeaponMesh->SetRelativeTransform(A.WeaponAttachOffset);

	if (A.CameraSocket.IsNone())
	{
		DriverMesh->SetRelativeTransform(A.DriverRelativeToCamera);
	}
	else
	{
		DriverMesh->TickAnimation(0.f, false);
		DriverMesh->RefreshBoneTransforms();
		UpdateDriverPlacement();
	}

	bAnimated = true;
	return true;
}

UZSFPAnimInstance* AZSWeaponBase::GetArmsAnim() const
{
	return DriverMesh ? Cast<UZSFPAnimInstance>(DriverMesh->GetAnimInstance()) : nullptr;
}

UZSFPAnimInstance* AZSWeaponBase::GetWeaponAnim() const
{
	return WeaponMesh ? Cast<UZSFPAnimInstance>(WeaponMesh->GetAnimInstance()) : nullptr;
}

void AZSWeaponBase::UpdateDriverPlacement()
{
	if (!bAnimated && !DriverMesh->GetSkinnedAsset())
	{
		return;
	}
	if (AnimSet.CameraSocket.IsNone() || !DriverMesh->DoesSocketExist(AnimSet.CameraSocket))
	{
		return;
	}
	// Camera = CameraFromSocket * Socket(CS) * Driver(rel) must equal the view root, so Driver(rel) is the inverse.
	const FTransform SocketCS = DriverMesh->GetSocketTransform(AnimSet.CameraSocket, RTS_Component);
	FTransform Rel = (AnimSet.CameraFromSocket * SocketCS).Inverse();
	Rel.SetScale3D(FVector::OneVector);
	DriverMesh->SetRelativeTransform(Rel);
}

void AZSWeaponBase::PlayShotEffects(bool bAimed)
{
	if (!bAnimated)
	{
		return;
	}
	if (Loaded.FireSounds.Num() > 0)
	{
		USoundBase* Sound = Loaded.FireSounds[FMath::RandRange(0, Loaded.FireSounds.Num() - 1)];
		UGameplayStatics::SpawnSoundAttached(Sound, WeaponMesh, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true,
			1.f, FMath::FRandRange(0.97f, 1.03f));
	}
	if (UZSFPAnimInstance* GunAnim = GetWeaponAnim())
	{
		GunAnim->PlayAction(Loaded.WeaponFire, 1.f, 0.02f, 0.08f);
	}
	if (Loaded.MuzzleFlash)
	{
		const bool bSocket = WeaponMesh->DoesSocketExist(AnimSet.MuzzleSocket);
		UNiagaraFunctionLibrary::SpawnSystemAttached(Loaded.MuzzleFlash, WeaponMesh, bSocket ? AnimSet.MuzzleSocket : NAME_None,
			bSocket ? FVector::ZeroVector : AnimSet.MuzzleOffset, FRotator::ZeroRotator,
			bSocket ? EAttachLocation::SnapToTarget : EAttachLocation::KeepRelativeOffset, true);
	}
}

void AZSWeaponBase::PlayFireTail()
{
	if (bAnimated && Loaded.FireTail)
	{
		UGameplayStatics::SpawnSoundAttached(Loaded.FireTail, WeaponMesh);
	}
}

void AZSWeaponBase::PlayDryFire()
{
	USoundBase* Sound = bAnimated ? Loaded.DryFire : DryFireSound.Get();
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}
