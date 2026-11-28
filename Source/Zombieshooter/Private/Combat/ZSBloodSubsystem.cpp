#include "Combat/ZSBloodSubsystem.h"
#include "Combat/ZSGoreSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Zombieshooter.h"

namespace
{
	const TCHAR* MatPath = TEXT("/Game/ZS/Materials/");
	const TCHAR* CuePath = TEXT("/Game/RealWeaponSoundFX/Bullet_Sound_FX/Combained_Cues/");

	// Linear colours (the sprite material is lit).
	const FLinearColor MistColor(0.07f, 0.0025f, 0.002f);
	const FLinearColor DropColor(0.12f, 0.003f, 0.002f);
	const FLinearColor DustColor(0.42f, 0.39f, 0.35f);
	const FLinearColor DirtColor(0.2f, 0.15f, 0.1f);
	const FLinearColor ChipColor(0.08f, 0.075f, 0.07f);
	const FLinearColor WoodColor(0.3f, 0.2f, 0.11f);
	const FLinearColor SparkColor(1.f, 0.55f, 0.18f);

	UMaterialInterface* LoadMat(const TCHAR* Name)
	{
		return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s%s.%s"), MatPath, Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	USoundBase* LoadCue(const TCHAR* Name)
	{
		return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("%s%s.%s"), CuePath, Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
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

UInstancedStaticMeshComponent* UZSBloodSubsystem::MakeISM(const TCHAR* Name, UMaterialInterface* Material)
{
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(Host, Name);
	ISM->SetupAttachment(Host->GetRootComponent());
	ISM->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->SetCastShadow(false);
	ISM->SetMobility(EComponentMobility::Movable);
	ISM->NumCustomDataFloats = 4;   // opacity, r, g, b
	ISM->bUseAsOccluder = false;
	ISM->SetTranslucentSortPriority(10);
	if (Material)
	{
		ISM->SetMaterial(0, Material);
	}
	ISM->RegisterComponent();
	return ISM;
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

	PuffISM = MakeISM(TEXT("FXPuffs"), LoadMat(TEXT("MI_ZS_FXPuff")));
	DropISM = MakeISM(TEXT("FXDrops"), LoadMat(TEXT("MI_ZS_FXDrop")));
	SparkISM = MakeISM(TEXT("FXSparks"), LoadMat(TEXT("M_ZS_FXSpark")));
	SplatMaterial = LoadMat(TEXT("MI_ZS_BloodSplat"));
	PoolMaterial = LoadMat(TEXT("MI_ZS_BloodPool"));
	HoleMaterial = LoadMat(TEXT("MI_ZS_BulletHole"));
	if (!SplatMaterial || !HoleMaterial)
	{
		UE_LOG(LogZS, Warning, TEXT("Gore FX materials missing - run Tools/Import/create_gore_fx.py."));
	}

	ImpactSounds.Add(uint8(EZSSurface::Concrete), LoadCue(TEXT("Cue_Bullet_on_stone")));
	ImpactSounds.Add(uint8(EZSSurface::Metal), LoadCue(TEXT("Cue_Bullet_on_metal")));
	ImpactSounds.Add(uint8(EZSSurface::Wood), LoadCue(TEXT("Cue_Bullet_on_wood")));
	ImpactSounds.Add(uint8(EZSSurface::Glass), LoadCue(TEXT("Cue_Bullet_on_glass_small")));
	ImpactSounds.Add(uint8(EZSSurface::Dirt), LoadCue(TEXT("Cue_Bullet_on_ground")));
	FleshSound = LoadCue(TEXT("Cue_Bullet_on_body"));
}

UZSBloodSubsystem::FParticle* UZSBloodSubsystem::AddParticle(EKind Kind)
{
	if (Particles.Num() >= MaxParticles)
	{
		// Recycle the oldest-looking one rather than dropping new (new effects are what you're looking at).
		Particles.RemoveAtSwap(0, EAllowShrinking::No);
	}
	FParticle& P = Particles.AddDefaulted_GetRef();
	P.Kind = Kind;
	return &P;
}

// ---- Flesh -----------------------------------------------------------------------------------

void UZSBloodSubsystem::SpawnHitBlood(const FVector& Location, const FVector& Direction, float Intensity)
{
	if (!UZSGoreSubsystem::BloodEnabled())
	{
		return;
	}
	EnsureResources();
	UWorld* World = GetWorld();
	const FVector Dir = Direction.GetSafeNormal();
	const float Gore = UZSGoreSubsystem::DismemberEnabled() ? 1.f : 0.6f;

	// Exit mist: a red puff blown out the back, a smaller one toward the shooter. Capped so sustained fire doesn't
	// build a pink cloud.
	int32 LiveMist = 0;
	for (const FParticle& Pt : Particles) { LiveMist += (Pt.Kind == EKind::Puff && Pt.Color.G < 0.02f && Pt.Color.R > Pt.Color.G * 5.f); }
	const int32 Mists = LiveMist > 18 ? 0 : FMath::RoundToInt((2.f + 1.f * Intensity) * Gore);
	for (int32 i = 0; i < Mists; ++i)
	{
		FParticle* P = AddParticle(EKind::Puff);
		const bool bBack = i == 0;
		P->Location = Location + FMath::VRand() * 3.f;
		P->Velocity = FMath::VRandCone(bBack ? -Dir : Dir, FMath::DegreesToRadians(bBack ? 40.f : 28.f)) * FMath::FRandRange(80.f, 320.f) * (bBack ? 0.5f : 1.f);
		P->Size = FMath::FRandRange(4.f, 8.f);
		P->EndSize = P->Size * FMath::FRandRange(2.5f, 4.f) * FMath::Sqrt(Intensity) * (bBack ? 0.6f : 1.f);
		P->MaxLife = FMath::FRandRange(0.25f, 0.5f);
		P->Drag = 6.f;
		P->Gravity = 0.25f;
		P->Opacity = FMath::FRandRange(0.35f, 0.55f);
		P->Color = MistColor * FMath::FRandRange(0.6f, 1.f);
	}
	// Droplets: fast, mostly out of the exit side, falling under gravity.
	const int32 Drops = FMath::RoundToInt((10.f + 12.f * Intensity) * Gore);
	for (int32 i = 0; i < Drops; ++i)
	{
		FParticle* P = AddParticle(EKind::Drop);
		const bool bBack = i % 5 == 0;
		P->Location = Location;
		P->Velocity = FMath::VRandCone(bBack ? -Dir : Dir, FMath::DegreesToRadians(bBack ? 35.f : 22.f)) * FMath::FRandRange(250.f, 750.f)
			+ FVector(0.f, 0.f, FMath::FRandRange(20.f, 140.f));
		P->Size = FMath::FRandRange(0.8f, 2.4f);
		P->EndSize = P->Size * 0.6f;
		P->MaxLife = FMath::FRandRange(0.4f, 0.9f);
		P->Gravity = 1.f;
		P->Drag = 0.6f;
		P->Stretch = 0.012f;
		P->Opacity = 1.f;
		P->Color = DropColor * FMath::FRandRange(0.8f, 1.3f);
	}

	// Splat on whatever is behind the target (wall, floor) within 3.5 m, and drips on the floor below.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSBlood), false);
	FHitResult Hit;
	const FVector Start = Location + Dir * 30.f;
	const FVector End = Start + (Dir + FVector(0.f, 0.f, -0.3f)).GetSafeNormal() * 350.f;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && !(Hit.GetActor() && Hit.GetActor()->IsA<APawn>()))
	{
		const float Near = 1.f - Hit.Time * 0.5f;
		SpawnSplat(Hit.ImpactPoint, Hit.ImpactNormal, FMath::FRandRange(40.f, 75.f) * Near * FMath::Sqrt(Intensity));
	}
	if (FMath::FRand() < 0.5f * Intensity && World->LineTraceSingleByChannel(Hit, Location, Location - FVector(0.f, 0.f, 250.f), ECC_Visibility, Params)
		&& !(Hit.GetActor() && Hit.GetActor()->IsA<APawn>()))
	{
		SpawnSplat(Hit.ImpactPoint + FVector(Dir.X, Dir.Y, 0.f) * FMath::FRandRange(10.f, 60.f), Hit.ImpactNormal, FMath::FRandRange(18.f, 35.f));
	}

	const double Now = World->GetTimeSeconds();
	if (FleshSound && Now - LastFleshSoundTime > 0.035)
	{
		LastFleshSoundTime = Now;
		UGameplayStatics::PlaySoundAtLocation(this, FleshSound, Location, FMath::Clamp(0.6f + 0.25f * Intensity, 0.6f, 1.2f), FMath::FRandRange(0.85f, 1.05f));
	}
}

void UZSBloodSubsystem::SpawnDrip(const FVector& Location, const FVector& Velocity, int32 Count)
{
	if (!UZSGoreSubsystem::BloodEnabled())
	{
		return;
	}
	EnsureResources();
	for (int32 i = 0; i < Count; ++i)
	{
		FParticle* P = AddParticle(EKind::Drop);
		P->Location = Location;
		P->Velocity = Velocity + FMath::VRand() * 60.f;
		P->Size = FMath::FRandRange(0.9f, 2.f);
		P->EndSize = P->Size;
		P->MaxLife = FMath::FRandRange(0.5f, 0.9f);
		P->Gravity = 1.f;
		P->Drag = 0.3f;
		P->Stretch = 0.012f;
		P->Color = DropColor;
	}
	// A little mist with it.
	FParticle* M = AddParticle(EKind::Puff);
	M->Location = Location;
	M->Velocity = Velocity * 0.3f;
	M->Size = 4.f;
	M->EndSize = 18.f;
	M->MaxLife = 0.4f;
	M->Drag = 4.f;
	M->Opacity = 0.5f;
	M->Color = MistColor;
}

void UZSBloodSubsystem::SpawnSmoke(const FVector& Location, const FVector& Velocity, float StartSize, float EndSize, float Life, const FLinearColor& Color, float Opacity)
{
	EnsureResources();
	FParticle* P = AddParticle(EKind::Puff);
	P->Location = Location;
	P->Velocity = Velocity;
	P->Size = StartSize;
	P->EndSize = EndSize;
	P->MaxLife = Life;
	P->Drag = 0.08f;
	P->Gravity = -0.01f;
	P->Opacity = Opacity;
	P->Color = Color;
}

void UZSBloodSubsystem::SpawnEmber(const FVector& Location, const FVector& Velocity)
{
	EnsureResources();
	FParticle* P = AddParticle(EKind::Spark);
	P->Location = Location;
	P->Velocity = Velocity;
	P->Size = FMath::FRandRange(1.2f, 2.6f);
	P->EndSize = P->Size * 0.3f;
	P->MaxLife = FMath::FRandRange(2.f, 4.f);
	P->Gravity = -0.04f;
	P->Drag = 0.5f;
	P->Stretch = 0.004f;
	P->Opacity = 0.9f;
	P->Color = FLinearColor(1.f, 0.42f, 0.1f);
}

void UZSBloodSubsystem::SpawnDust(const FVector& Location, const FVector& Velocity, float Scale)
{
	EnsureResources();
	FParticle* P = AddParticle(EKind::Puff);
	P->Location = Location + FMath::VRand() * 8.f;
	P->Velocity = Velocity + FMath::VRand() * 40.f + FVector(0.f, 0.f, 30.f);
	P->Size = 10.f * Scale;
	P->EndSize = FMath::FRandRange(45.f, 70.f) * Scale;
	P->MaxLife = FMath::FRandRange(0.6f, 1.0f);
	P->Drag = 3.f;
	P->Gravity = -0.02f;
	P->Opacity = 0.3f;
	P->Color = DustColor * 0.8f;
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
		if (FDecalSlot* Slot = NextSlot(BloodDecals, NextBlood, MaxBloodDecals, PoolMaterial ? PoolMaterial.Get() : SplatMaterial.Get()))
		{
			FRotator Rot = (-Hit.ImpactNormal).Rotation();
			Rot.Roll = FMath::FRandRange(0.f, 360.f);
			Slot->Decal->SetWorldLocationAndRotation(Hit.ImpactPoint, Rot);
			Slot->MID->SetScalarParameterValue(TEXT("Frame"), 0.f);
			Slot->TargetSize = FMath::FRandRange(55.f, 90.f);
			Slot->Grow = 0.f;   // pools spread slowly
			Slot->GrowRate = 0.3f;
			Slot->Decal->DecalSize = FVector(10.f, 1.f, 1.f);
			Slot->Decal->MarkRenderStateDirty();
		}
	}
}

UZSBloodSubsystem::FDecalSlot* UZSBloodSubsystem::NextSlot(TArray<FDecalSlot>& Pool, int32& Next, int32 Max, UMaterialInterface* Material)
{
	if (!Material || !Host)
	{
		return nullptr;
	}
	FDecalSlot* Slot = nullptr;
	if (Pool.Num() < Max)
	{
		Slot = &Pool.AddDefaulted_GetRef();
		Slot->Decal = NewObject<UDecalComponent>(Host);
		Slot->Decal->SetupAttachment(Host->GetRootComponent());
		Slot->Decal->SetFadeScreenSize(0.0004f);
		Slot->Decal->RegisterComponent();
	}
	else
	{
		Slot = &Pool[Next];
		Next = (Next + 1) % Pool.Num();
	}
	if (!Slot->MID || Slot->MID->Parent != Material)
	{
		Slot->MID = UMaterialInstanceDynamic::Create(Material, Slot->Decal);
		Slot->Decal->SetDecalMaterial(Slot->MID);
	}
	Slot->Grow = 1.f;
	return Slot;
}

void UZSBloodSubsystem::SpawnSplat(const FVector& Location, const FVector& Normal, float Size)
{
	EnsureResources();
	FDecalSlot* Slot = NextSlot(BloodDecals, NextBlood, MaxBloodDecals, SplatMaterial);
	if (!Slot)
	{
		return;
	}
	// Decals project along X: point it into the surface, random spin around the normal.
	FRotator Rot = (-Normal).Rotation();
	Rot.Roll = FMath::FRandRange(0.f, 360.f);
	Slot->Decal->SetWorldLocationAndRotation(Location, Rot);
	Slot->Decal->SortOrder = NextBlood;
	Slot->MID->SetScalarParameterValue(TEXT("Frame"), float(FMath::RandRange(0, 3)));
	Slot->TargetSize = Size;
	Slot->Grow = 0.6f;   // splats hit the wall almost at once, then run out a little
	Slot->GrowRate = 2.5f;
	Slot->Decal->DecalSize = FVector(12.f, Size * 0.6f, Size * 0.6f);
	Slot->Decal->MarkRenderStateDirty();
}

// ---- World impacts ----------------------------------------------------------------------------

EZSSurface UZSBloodSubsystem::SurfaceOf(const FHitResult& Hit)
{
	const UPrimitiveComponent* Comp = Hit.GetComponent();
	if (!Comp)
	{
		return EZSSurface::Concrete;
	}
	FString Name;
	if (const UMaterialInterface* M = Comp->GetMaterial(0))
	{
		Name = M->GetName();
	}
	if (const AActor* A = Hit.GetActor())
	{
		Name += TEXT(" ") + A->GetName();
	}
	Name += TEXT(" ") + Comp->GetName();
	Name.ToLowerInline();
	auto Has = [&Name](std::initializer_list<const TCHAR*> Words)
	{
		for (const TCHAR* W : Words) { if (Name.Contains(W)) { return true; } }
		return false;
	};
	if (Has({ TEXT("glass"), TEXT("window") })) { return EZSSurface::Glass; }
	if (Has({ TEXT("metal"), TEXT("rust"), TEXT("steel"), TEXT("iron"), TEXT("car"), TEXT("pipe"), TEXT("tank"), TEXT("container"),
		TEXT("corrugated"), TEXT("vent"), TEXT("grate"), TEXT("locker"), TEXT("door"), TEXT("lamp"), TEXT("crate") })) { return EZSSurface::Metal; }
	if (Has({ TEXT("wood"), TEXT("plank"), TEXT("board"), TEXT("pallet"), TEXT("tree"), TEXT("bark") })) { return EZSSurface::Wood; }
	if (Has({ TEXT("dirt"), TEXT("ground"), TEXT("grass"), TEXT("soil"), TEXT("mud"), TEXT("debris"), TEXT("sand"), TEXT("landscape") })) { return EZSSurface::Dirt; }
	return EZSSurface::Concrete;
}

void UZSBloodSubsystem::SpawnImpact(const FHitResult& Hit, const FVector& Direction, bool bPlaySound)
{
	EnsureResources();
	const EZSSurface Surface = SurfaceOf(Hit);
	const FVector N = Hit.ImpactNormal;
	const FVector P0 = Hit.ImpactPoint + N * 1.5f;
	const FVector Reflect = FMath::GetReflectionVector(Direction.GetSafeNormal(), N);

	// Bullet hole (not on glass: it'd float on the transparent pane).
	if (Surface != EZSSurface::Glass && Surface != EZSSurface::Dirt)
	{
		if (FDecalSlot* Slot = NextSlot(HoleDecals, NextHole, MaxHoleDecals, HoleMaterial))
		{
			FRotator Rot = (-N).Rotation();
			Rot.Roll = FMath::FRandRange(0.f, 360.f);
			Slot->Decal->SetWorldLocationAndRotation(Hit.ImpactPoint, Rot);
			const float S = Surface == EZSSurface::Metal ? FMath::FRandRange(3.f, 4.5f) : FMath::FRandRange(5.f, 8.f);
			Slot->Decal->DecalSize = FVector(4.f, S, S);
			Slot->TargetSize = S;
			Slot->Grow = 1.f;
			Slot->MID->SetScalarParameterValue(TEXT("Frame"), float(FMath::RandRange(0, 3)));
			Slot->MID->SetScalarParameterValue(TEXT("Metal"), Surface == EZSSurface::Metal ? 0.8f : 0.f);
			Slot->Decal->MarkRenderStateDirty();
		}
	}

	// Dust / dirt plume off the surface.
	const bool bDusty = Surface == EZSSurface::Concrete || Surface == EZSSurface::Dirt || Surface == EZSSurface::Wood;
	const FLinearColor Puff = Surface == EZSSurface::Dirt ? DirtColor : (Surface == EZSSurface::Wood ? WoodColor : DustColor);
	const int32 Puffs = bDusty ? 4 : 2;
	for (int32 i = 0; i < Puffs; ++i)
	{
		FParticle* P = AddParticle(EKind::Puff);
		P->Location = P0;
		P->Velocity = FMath::VRandCone((N * 2.f + Reflect).GetSafeNormal(), FMath::DegreesToRadians(30.f)) * FMath::FRandRange(60.f, 260.f);
		P->Size = FMath::FRandRange(4.f, 8.f);
		P->EndSize = P->Size * FMath::FRandRange(4.f, 7.f);
		P->MaxLife = FMath::FRandRange(0.6f, 1.3f);
		P->Drag = 3.5f;
		P->Gravity = -0.03f;
		P->Opacity = bDusty ? FMath::FRandRange(0.35f, 0.6f) : 0.18f;
		P->Color = Puff * FMath::FRandRange(0.85f, 1.15f);
	}
	// Chips / splinters / glass shards: little dark specks that fall.
	const int32 Chips = Surface == EZSSurface::Metal ? 0 : 6;
	for (int32 i = 0; i < Chips; ++i)
	{
		FParticle* P = AddParticle(EKind::Drop);
		P->Location = P0;
		P->Velocity = FMath::VRandCone((N + Reflect * 0.6f).GetSafeNormal(), FMath::DegreesToRadians(40.f)) * FMath::FRandRange(200.f, 550.f);
		P->Size = FMath::FRandRange(0.5f, 1.3f);
		P->EndSize = P->Size;
		P->MaxLife = FMath::FRandRange(0.4f, 0.8f);
		P->Gravity = 1.f;
		P->Stretch = 0.004f;
		P->Color = Surface == EZSSurface::Glass ? FLinearColor(0.6f, 0.65f, 0.7f) : (Surface == EZSSurface::Wood ? WoodColor : ChipColor);
	}
	// Sparks off metal (and the odd one off concrete).
	const int32 Sparks = Surface == EZSSurface::Metal ? FMath::RandRange(6, 11) : (Surface == EZSSurface::Concrete && FMath::FRand() < 0.3f ? 2 : 0);
	for (int32 i = 0; i < Sparks; ++i)
	{
		FParticle* P = AddParticle(EKind::Spark);
		P->Location = P0;
		P->Velocity = FMath::VRandCone((Reflect + N * 0.5f).GetSafeNormal(), FMath::DegreesToRadians(35.f)) * FMath::FRandRange(500.f, 1400.f);
		P->Size = FMath::FRandRange(0.35f, 0.7f);
		P->EndSize = P->Size * 0.5f;
		P->MaxLife = FMath::FRandRange(0.12f, 0.35f);
		P->Gravity = 0.8f;
		P->Drag = 1.5f;
		P->Stretch = 0.02f;
		P->Color = SparkColor;
	}
	if (bPlaySound)
	{
		PlayImpactSound(Surface, Hit.ImpactPoint);
	}
}

void UZSBloodSubsystem::PlayImpactSound(EZSSurface Surface, const FVector& Location)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastImpactSoundTime < 0.045)
	{
		return;
	}
	LastImpactSoundTime = Now;
	if (const TObjectPtr<USoundBase>* S = ImpactSounds.Find(uint8(Surface)); S && *S)
	{
		// Quieter with distance from the listener (the cues are 2D one-shots).
		float Volume = 0.55f;
		if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			Volume *= FMath::GetMappedRangeValueClamped(FVector2D(300.f, 3500.f), FVector2D(1.f, 0.15f), FVector::Dist(Cam->GetCameraLocation(), Location));
		}
		UGameplayStatics::PlaySoundAtLocation(this, *S, Location, Volume, FMath::FRandRange(0.9f, 1.1f));
	}
}

// ---- Tick ------------------------------------------------------------------------------------

void UZSBloodSubsystem::UpdateISM(UInstancedStaticMeshComponent* ISM, EKind Kind, const FVector& CamLoc)
{
	if (!ISM)
	{
		return;
	}
	TArray<FTransform> Transforms;
	TArray<float> Custom;
	for (const FParticle& P : Particles)
	{
		if (P.Kind != Kind)
		{
			continue;
		}
		const float A = P.Life / P.MaxLife;
		const float Size = FMath::Lerp(P.Size, P.EndSize, 1.f - FMath::Square(1.f - A));
		FVector ToCam = (CamLoc - P.Location).GetSafeNormal();
		if (ToCam.IsNearlyZero()) { ToCam = FVector::UpVector; }
		FVector Along = P.Velocity;
		float Length = Size;
		if (P.Stretch > 0.f && !Along.IsNearlyZero())
		{
			Length = Size + P.Velocity.Size() * P.Stretch * Size;
		}
		else
		{
			Along = FVector::CrossProduct(ToCam, FVector::UpVector);   // any axis in the plane; spin by life for puffs
			Along = Along.RotateAngleAxis(P.Life * 40.f + (P.MaxLife * 997.f), ToCam);
			if (Along.IsNearlyZero()) { Along = FVector::ForwardVector; }
		}
		const FMatrix M = FRotationMatrix::MakeFromZX(ToCam, Along);
		// Plane mesh is 100 x 100 cm.
		Transforms.Emplace(M.Rotator(), P.Location, FVector(Length / 100.f, Size / 100.f, 1.f));
		float Fade = Kind == EKind::Puff ? (1.f - A) * FMath::Min(1.f, A * 12.f) : (1.f - A * A);
		Custom.Add(P.Opacity * Fade);
		Custom.Add(P.Color.R);
		Custom.Add(P.Color.G);
		Custom.Add(P.Color.B);
	}

	if (ISM->GetInstanceCount() != Transforms.Num())
	{
		ISM->ClearInstances();
		if (Transforms.Num() > 0)
		{
			ISM->AddInstances(Transforms, false, true, false);
		}
	}
	else if (Transforms.Num() > 0)
	{
		ISM->BatchUpdateInstancesTransforms(0, Transforms, true, false, true);
	}
	for (int32 i = 0; i < Transforms.Num(); ++i)
	{
		ISM->SetCustomData(i, TArrayView<const float>(&Custom[i * 4], 4), false);
	}
	if (Transforms.Num() > 0)
	{
		ISM->MarkRenderStateDirty();
	}
}

void UZSBloodSubsystem::Tick(float DeltaTime)
{
	if (!Host)
	{
		return;
	}
	const float G = -980.f;
	for (int32 i = Particles.Num() - 1; i >= 0; --i)
	{
		FParticle& P = Particles[i];
		P.Life += DeltaTime;
		if (P.Life >= P.MaxLife)
		{
			Particles.RemoveAtSwap(i, EAllowShrinking::No);
			continue;
		}
		P.Velocity.Z += G * P.Gravity * DeltaTime;
		P.Velocity *= FMath::Max(0.f, 1.f - P.Drag * DeltaTime);
		P.Location += P.Velocity * DeltaTime;
	}

	// Growing decals.
	auto GrowPool = [DeltaTime](TArray<FDecalSlot>& Pool)
	{
		for (FDecalSlot& S : Pool)
		{
			if (S.Grow < 1.f && S.Decal)
			{
				S.Grow = FMath::Min(1.f, S.Grow + DeltaTime * S.GrowRate);
				const float E = 1.f - FMath::Square(1.f - S.Grow);
				S.Decal->DecalSize = FVector(S.Decal->DecalSize.X, S.TargetSize * E, S.TargetSize * E);
				S.Decal->MarkRenderStateDirty();
			}
		}
	};
	GrowPool(BloodDecals);

	FVector CamLoc = FVector::ZeroVector;
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		CamLoc = Cam->GetCameraLocation();
	}
	UpdateISM(PuffISM, EKind::Puff, CamLoc);
	UpdateISM(DropISM, EKind::Drop, CamLoc);
	UpdateISM(SparkISM, EKind::Spark, CamLoc);
}
