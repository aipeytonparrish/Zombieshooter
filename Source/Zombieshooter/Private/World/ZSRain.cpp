#include "World/ZSRain.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "Sound/SoundBase.h"

AZSRain::AZSRain()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	Drops = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Drops"));
	RootComponent = Drops;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	Drops->SetStaticMesh(Plane.Object);
	Drops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Drops->SetCastShadow(false);
	Drops->SetMobility(EComponentMobility::Movable);
	Drops->bReceivesDecals = false;
	Drops->SetGenerateOverlapEvents(false);
	Drops->bAffectDistanceFieldLighting = false;
	Drops->SetCanEverAffectNavigation(false);
}

void AZSRain::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ZS/Materials/M_ZS_Rain.M_ZS_Rain"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Mat)
	{
		UE_LOG(LogTemp, Warning, TEXT("AZSRain: M_ZS_Rain missing (run Tools/Import/create_fx_materials.py)."));
		SetActorHiddenInGame(true);
		SetActorTickEnabled(false);
		return;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	// The fall is simulated here; switch the material's GPU fall off.
	MID->SetScalarParameterValue(TEXT("Speed"), 0.f);
	MID->SetScalarParameterValue(TEXT("Height"), 0.f);
	MID->SetScalarParameterValue(TEXT("Wind"), 0.f);
	Drops->SetMaterial(0, MID);

	// Drops live in world space inside a box around the camera and wrap around it as it moves.
	FRandomStream Rand(1337);
	const FVector Center = GetActorLocation();
	Positions.SetNum(DropCount);
	Yaws.SetNum(DropCount);
	TArray<FTransform> Xforms;
	Xforms.Reserve(DropCount);
	for (int32 i = 0; i < DropCount; ++i)
	{
		Positions[i] = Center + FVector(Rand.FRandRange(-BoxExtent.X, BoxExtent.X), Rand.FRandRange(-BoxExtent.Y, BoxExtent.Y), Rand.FRandRange(-BoxExtent.Z, BoxExtent.Z));
		Yaws[i] = Rand.FRandRange(0.f, 180.f);
		Xforms.Add(DropTransform(i));
	}
	Drops->AddInstances(Xforms, false, true, false);
	Drops->SetBoundsScale(2.f);

	if (bLightning)
	{
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It && !Sun; ++It)
		{
			Sun = Cast<UDirectionalLightComponent>(It->GetLightComponent());
		}
		for (TActorIterator<ASkyLight> It(GetWorld()); It && !Sky; ++It)
		{
			Sky = It->GetLightComponent();
		}
		SunBase = Sun ? Sun->Intensity : 0.f;
		SkyBase = Sky ? Sky->Intensity : 0.f;
		NextFlash = FMath::FRandRange(6.f, LightningInterval.X);
		if (!ThunderSound)
		{
			ThunderSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/GunSoundsPackVol1/Bonus/cue/Explosion_Large_3-1_Cue.Explosion_Large_3-1_Cue"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
	}
}

bool AZSRain::IsSheltered(const FVector& P) const
{
	for (const FBox& B : ShelterBoxes)
	{
		if (B.IsInside(P))
		{
			return true;
		}
	}
	return false;
}

void AZSRain::UpdateLightning(float DeltaSeconds)
{
	if (!bLightning || (!Sun && !Sky))
	{
		return;
	}
	if (ThunderDelay >= 0.f)
	{
		ThunderDelay -= DeltaSeconds;
		if (ThunderDelay < 0.f && ThunderSound)
		{
			// Distant thunder: a big explosion pitched way down.
			UGameplayStatics::PlaySound2D(this, ThunderSound, FMath::FRandRange(0.35f, 0.6f), FMath::FRandRange(0.35f, 0.5f));
		}
	}
	if (FlashTime < 0.f)
	{
		NextFlash -= DeltaSeconds;
		if (NextFlash <= 0.f)
		{
			FlashTime = 0.f;
			NextFlash = FMath::FRandRange(LightningInterval.X, LightningInterval.Y);
			ThunderDelay = FMath::FRandRange(0.4f, 2.2f);
		}
		return;
	}
	// Double flicker over ~0.45 s.
	FlashTime += DeltaSeconds;
	const float T = FlashTime;
	float A = 0.f;
	if (T < 0.07f) { A = 1.f; }
	else if (T < 0.14f) { A = 0.15f; }
	else if (T < 0.24f) { A = 0.8f; }
	else if (T < 0.45f) { A = 0.8f * (1.f - (T - 0.24f) / 0.21f); }
	else { FlashTime = -1.f; }
	if (Sun) { Sun->SetIntensity(SunBase * (1.f + LightningBoost * A)); }
	if (Sky) { Sky->SetIntensity(SkyBase * (1.f + LightningBoost * 0.5f * A)); }
}

FTransform AZSRain::DropTransform(int32 i) const
{
	// Engine plane is 100x100 in XY facing +Z: pitch it upright (local X vertical), lean it with the wind.
	const float Lean = FMath::RadiansToDegrees(FMath::Atan(Wind));
	return FTransform(FRotator(90.f - Lean, Yaws[i], 0.f), Positions[i], FVector(StreakSize.X / 100.f, StreakSize.Y / 100.f, 1.f));
}

void AZSRain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam || Positions.Num() == 0)
	{
		return;
	}
	UpdateLightning(DeltaSeconds);
	const FVector C = Cam->GetCameraLocation();
	SetActorLocation(C);   // keeps the component bounds around the camera

	const FVector Step(FallSpeed * Wind * DeltaSeconds, 0.f, -FallSpeed * DeltaSeconds);
	auto Wrap = [](double V, double Mid, double Ext) { const double D = V - Mid; return D > Ext ? V - 2.0 * Ext : (D < -Ext ? V + 2.0 * Ext : V); };
	TArray<FTransform> Xforms;
	Xforms.SetNum(Positions.Num());
	for (int32 i = 0; i < Positions.Num(); ++i)
	{
		FVector& P = Positions[i];
		P += Step;
		P.X = Wrap(P.X, C.X, BoxExtent.X);
		P.Y = Wrap(P.Y, C.Y, BoxExtent.Y);
		P.Z = Wrap(P.Z, C.Z, BoxExtent.Z);
		Xforms[i] = DropTransform(i);
		if (ShelterBoxes.Num() > 0 && IsSheltered(P))
		{
			Xforms[i].SetScale3D(FVector::ZeroVector);
		}
	}
	Drops->BatchUpdateInstancesTransforms(0, Xforms, true, true, true);
}
