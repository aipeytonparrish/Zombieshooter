#include "Combat/ZSGoreSubsystem.h"
#include "Combat/ZSBloodSubsystem.h"
#include "Core/ZSGameUserSettings.h"
#include "UI/ZSHUD.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"

int32 UZSGoreSubsystem::GetGoreLevel()
{
	const UZSGameUserSettings* S = UZSGameUserSettings::Get();
	return S ? FMath::Clamp(S->GoreLevel, 0, 2) : 2;
}

bool UZSGoreSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->IsGameWorld());
}

TStatId UZSGoreSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZSGoreSubsystem, STATGROUP_Tickables);
}

void UZSGoreSubsystem::SpawnGibs(const FVector& Location, const FVector& Direction, int32 Count, float Scale)
{
	if (!DismemberEnabled())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!Host)
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
		Host->SetRootComponent(NewObject<USceneComponent>(Host, TEXT("Root")));
		Host->GetRootComponent()->RegisterComponent();
	}
	static UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	// Our wet flesh material (create_gore_fx.py); the engine's plain shape material if it's missing.
	static UMaterialInterface* Flesh = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ZS/Materials/M_ZS_Flesh.M_ZS_Flesh"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* Shape = Flesh ? nullptr : LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	for (int32 i = 0; i < Count; ++i)
	{
		// Recycle the oldest past the cap.
		while (Gibs.Num() >= MaxGibs)
		{
			if (UStaticMeshComponent* Old = Gibs[0].Comp.Get()) { Old->DestroyComponent(); }
			Gibs.RemoveAt(0);
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Host, NAME_None, RF_Transient);
		C->SetStaticMesh(FMath::FRand() < 0.75f ? Sphere : Cube);
		const float S = FMath::FRandRange(0.05f, 0.13f) * Scale;
		C->SetWorldScale3D(FVector(S, S * FMath::FRandRange(0.6f, 1.4f), S * FMath::FRandRange(0.5f, 1.f)));
		C->SetWorldLocationAndRotation(Location + FMath::VRand() * 10.f, FMath::VRand().Rotation());
		C->SetCollisionProfileName(TEXT("PhysicsActor"));
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		C->SetCastShadow(false);
		C->SetMobility(EComponentMobility::Movable);
		if (Flesh)
		{
			C->SetMaterial(0, Flesh);
		}
		else if (Shape)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Shape, C);
			MID->SetVectorParameterValue(TEXT("Color"), FMath::RandBool() ? FLinearColor(0.18f, 0.01f, 0.01f) : FLinearColor(0.32f, 0.08f, 0.06f));
			C->SetMaterial(0, MID);
		}
		C->RegisterComponent();
		C->SetSimulatePhysics(true);
		C->SetMassOverrideInKg(NAME_None, 0.6f, true);
		const FVector V = (Direction.GetSafeNormal() * 0.6f + FMath::VRand() * 0.8f + FVector(0.f, 0.f, 0.7f)).GetSafeNormal() * FMath::FRandRange(250.f, 650.f);
		C->SetPhysicsLinearVelocity(V);
		C->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 720.f);
		Gibs.Add({ C, 10.f + FMath::FRand() * 5.f });
	}
	if (UZSBloodSubsystem* Blood = World->GetSubsystem<UZSBloodSubsystem>())
	{
		Blood->SpawnHitBlood(Location, Direction, 2.2f * Scale);
		Blood->SpawnHitBlood(Location, -Direction + FVector(0, 0, 0.5f), 1.4f * Scale);
	}
}

void UZSGoreSubsystem::AddStump(USkeletalMeshComponent* Mesh, FName Bone, float Duration)
{
	if (BloodEnabled() && Mesh)
	{
		Stumps.Add({ Mesh, Bone, Duration, 0.f });
	}
}

void UZSGoreSubsystem::SplatterNearCamera(const FVector& Location, float Radius, float Amount)
{
	if (!BloodEnabled())
	{
		return;
	}
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam)
	{
		return;
	}
	const FVector ToHit = Location - Cam->GetCameraLocation();
	const float Dist = ToHit.Size();
	if (Dist < Radius && FVector::DotProduct(ToHit.GetSafeNormal(), Cam->GetCameraRotation().Vector()) > 0.2f)
	{
		if (const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			AZSHUD::AddScreenBlood(P, Amount * (1.f - Dist / Radius * 0.6f));
		}
	}
}

void UZSGoreSubsystem::Tick(float DeltaTime)
{
	UZSBloodSubsystem* Blood = GetWorld()->GetSubsystem<UZSBloodSubsystem>();
	for (int32 i = Stumps.Num() - 1; i >= 0; --i)
	{
		FStump& S = Stumps[i];
		S.TimeLeft -= DeltaTime;
		S.NextSpurt -= DeltaTime;
		USkeletalMeshComponent* Mesh = S.Mesh.Get();
		if (!Mesh || S.TimeLeft <= 0.f)
		{
			Stumps.RemoveAtSwap(i);
			continue;
		}
		if (S.NextSpurt <= 0.f && Blood)
		{
			// Pulses, like a heartbeat, weakening over time.
			S.NextSpurt = 0.18f;
			const FTransform T = Mesh->GetSocketTransform(S.Bone);
			const FName Parent = Mesh->GetParentBone(S.Bone);
			const FVector Out = (T.GetLocation() - Mesh->GetSocketLocation(Parent)).GetSafeNormal();
			const float Pulse = FMath::Clamp(S.TimeLeft, 0.3f, 1.2f);
			Blood->SpawnDrip(T.GetLocation(), (Out + FVector(0, 0, 0.35f)).GetSafeNormal() * 260.f * Pulse, FMath::RoundToInt(3.f + 4.f * Pulse));
		}
	}
	for (int32 i = Gibs.Num() - 1; i >= 0; --i)
	{
		Gibs[i].Life -= DeltaTime;
		UStaticMeshComponent* C = Gibs[i].Comp.Get();
		if (!C || Gibs[i].Life <= 0.f)
		{
			if (C) { C->DestroyComponent(); }
			Gibs.RemoveAtSwap(i);
			continue;
		}
		// Flying chunks trail blood and leave a splat where they land.
		if (Blood && !Gibs[i].bSplatted)
		{
			const FVector V = C->GetPhysicsLinearVelocity();
			Gibs[i].DripTimer -= DeltaTime;
			if (V.SizeSquared() > 150.f * 150.f && Gibs[i].DripTimer <= 0.f)
			{
				Gibs[i].DripTimer = 0.05f;
				Blood->SpawnDrip(C->GetComponentLocation(), V * 0.2f, 1);
			}
			else if (V.SizeSquared() < 40.f * 40.f && Gibs[i].DripTimer < -0.4f)
			{
				Gibs[i].bSplatted = true;
				FHitResult Hit;
				const FVector L = C->GetComponentLocation();
				FCollisionQueryParams Q(SCENE_QUERY_STAT(ZSGibSplat), false);
				Q.AddIgnoredComponent(C);
				if (GetWorld()->LineTraceSingleByChannel(Hit, L + FVector(0, 0, 10), L - FVector(0, 0, 60), ECC_Visibility, Q))
				{
					Blood->SpawnSplat(Hit.ImpactPoint, Hit.ImpactNormal, FMath::FRandRange(14.f, 26.f));
				}
			}
		}
	}
}
