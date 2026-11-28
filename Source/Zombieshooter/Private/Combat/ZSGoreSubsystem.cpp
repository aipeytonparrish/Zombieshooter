#include "Combat/ZSGoreSubsystem.h"
#include "Weapons/ZSWeaponTypes.h"
#include "Combat/ZSBloodSubsystem.h"
#include "Core/ZSGameUserSettings.h"
#include "UI/ZSHUD.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/SkinnedAssetCommon.h"
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

void UZSGoreSubsystem::DestroyGib(FGib& G)
{
	// Child first: never leave a part attached to a destroyed carrier.
	if (UPoseableMeshComponent* P = G.Part.Get())
	{
		P->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		P->DestroyComponent();
	}
	if (UStaticMeshComponent* C = G.Comp.Get())
	{
		C->DestroyComponent();
	}
	G.Part = nullptr;
	G.Comp = nullptr;
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
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	// Our wet flesh material (create_gore_fx.py); the engine's plain shape material if it's missing.
	UMaterialInterface* Flesh = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ZS/Materials/M_ZS_Flesh.M_ZS_Flesh"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* Shape = Flesh ? nullptr : LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	for (int32 i = 0; i < Count; ++i)
	{
		// Recycle the oldest past the cap.
		while (Gibs.Num() >= MaxGibs)
		{
			DestroyGib(Gibs[0]);   // the severed part riding on it too (an orphaned part crashed the renderer)
			Gibs.RemoveAt(0);
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Host, NAME_None, RF_Transient);
		C->SetStaticMesh(FMath::FRand() < 0.75f ? Sphere : Cube);
		const float S = FMath::FRandRange(0.05f, 0.13f) * Scale;
		C->SetWorldScale3D(FVector(S, S * FMath::FRandRange(0.6f, 1.4f), S * FMath::FRandRange(0.5f, 1.f)));
		C->SetWorldLocationAndRotation(Location + FMath::VRand() * 10.f, FMath::VRand().Rotation());
		C->SetCollisionProfileName(TEXT("PhysicsActor"));
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		C->SetCollisionResponseToChannel(ZS_TRACE_WEAPON, ECR_Ignore);   // gibs must not stop bullets
		C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
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

void UZSGoreSubsystem::SpawnSeveredPart(USkeletalMeshComponent* Source, FName Bone, const FVector& Direction)
{
	if (!DismemberEnabled() || !Source || !Source->GetSkinnedAsset())
	{
		return;
	}
	const int32 CutIndex = Source->GetBoneIndex(Bone);
	if (CutIndex == INDEX_NONE)
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
	while (Gibs.Num() >= MaxGibs)
	{
		DestroyGib(Gibs[0]);
		Gibs.RemoveAt(0);
	}

	// Where the part is: from the cut bone towards the end of its chain.
	const TArray<FTransform>& CS = Source->GetComponentSpaceTransforms();
	const FTransform CompWorld = Source->GetComponentTransform();
	const FReferenceSkeleton& Ref = Source->GetSkinnedAsset()->GetRefSkeleton();
	const int32 NumBones = Ref.GetNum();
	auto InSubtree = [&Ref, CutIndex](int32 Index)
	{
		for (int32 I = Index; I != INDEX_NONE; I = Ref.GetParentIndex(I))
		{
			if (I == CutIndex) { return true; }
		}
		return false;
	};
	FVector Centre = FVector::ZeroVector;
	int32 Count = 0;
	for (int32 i = 0; i < NumBones && i < CS.Num(); ++i)
	{
		if (InSubtree(i)) { Centre += CompWorld.TransformPosition(CS[i].GetLocation()); ++Count; }
	}
	Centre = Count > 0 ? Centre / Count : CompWorld.TransformPosition(CS[CutIndex].GetLocation());
	const bool bHead = Bone == TEXT("head");

	// The part: the zombie's own mesh, posed like it was, every bone outside the part shrunk to nothing.
	// Pose it fully before it's registered (setting bones on a live poseable mesh marks it for render updates every
	// call; doing that in the same frame it was created tripped an engine render-state assertion).
	UPoseableMeshComponent* Part = NewObject<UPoseableMeshComponent>(Host, NAME_None, RF_Transient);
	Part->SetSkinnedAssetAndUpdate(Source->GetSkinnedAsset());
	for (int32 m = 0; m < Source->GetNumMaterials(); ++m) { Part->SetMaterial(m, Source->GetMaterial(m)); }
	Part->SetOverlayMaterial(Source->GetOverlayMaterial());
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetWorldTransform(CompWorld);
	const float Gone = 0.001f;
	if (Part->BoneSpaceTransforms.Num() == NumBones)
	{
		// Local (parent-relative) transforms from the source's component-space pose, with everything outside the
		// part shrunk to nothing.
		TArray<FTransform> Shaped;
		Shaped.SetNum(NumBones);
		for (int32 i = 0; i < NumBones && i < CS.Num(); ++i)
		{
			Shaped[i] = CS[i];
			Shaped[i].SetScale3D(InSubtree(i) ? CS[i].GetScale3D() : FVector(Gone));
			const int32 Parent = Ref.GetParentIndex(i);
			Part->BoneSpaceTransforms[i] = Parent == INDEX_NONE ? Shaped[i] : Shaped[i].GetRelativeTransform(Shaped[Parent]);
		}
	}
	Part->RegisterComponent();

	// Flies on a scripted arc (no physics: a physics carrier with the part attached crashed the renderer).
	FSevered S;
	S.Part = Part;
	S.Pivot = Centre;
	S.Offset = CompWorld.GetRelativeTransform(FTransform(Centre));
	S.Rotation = FQuat::Identity;
	S.Velocity = (Direction.GetSafeNormal() * 0.8f + FMath::VRand() * 0.35f + FVector(0.f, 0.f, 0.6f)).GetSafeNormal() * FMath::FRandRange(300.f, 560.f);
	S.Spin = FMath::VRand() * FMath::FRandRange(300.f, 700.f);
	S.Radius = bHead ? 12.f : 8.f;
	S.Life = 25.f;
	while (Severed.Num() >= 24)
	{
		if (UPoseableMeshComponent* Old = Severed[0].Part.Get()) { Old->DestroyComponent(); }
		Severed.RemoveAt(0);
	}
	Severed.Add(S);
}

FTransform UZSGoreSubsystem::JointTransform(USkeletalMeshComponent* Mesh, int32 BoneIndex)
{
	if (!Mesh || !Mesh->GetSkeletalMeshAsset() || BoneIndex == INDEX_NONE)
	{
		return Mesh ? Mesh->GetComponentTransform() : FTransform::Identity;
	}
	const FReferenceSkeleton& Ref = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
	TArray<int32, TInlineAllocator<8>> Chain;   // hidden bones from BoneIndex up, stopping below the first visible one
	int32 Live = BoneIndex;
	while (Live != INDEX_NONE && Mesh->IsBoneHidden(Live))
	{
		Chain.Add(Live);
		Live = Ref.GetParentIndex(Live);
	}
	FTransform T = Live != INDEX_NONE ? Mesh->GetBoneTransform(Live) : Mesh->GetComponentTransform();
	for (int32 i = Chain.Num() - 1; i >= 0; --i)
	{
		T = Ref.GetRefBonePose()[Chain[i]] * T;
	}
	return T;
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
			// A hidden (severed) bone's own transform is stale (frozen where it was cut, up to a metre off the body now).
			const int32 BoneIndex = Mesh->GetBoneIndex(S.Bone);
			const int32 ParentIndex = Mesh->GetBoneIndex(Mesh->GetParentBone(S.Bone));
			if (BoneIndex == INDEX_NONE || ParentIndex == INDEX_NONE)
			{
				continue;
			}
			const FVector Joint = JointTransform(Mesh, BoneIndex).GetLocation();
			const FVector Out = (Joint - JointTransform(Mesh, ParentIndex).GetLocation()).GetSafeNormal();
			const float Pulse = FMath::Clamp(S.TimeLeft, 0.3f, 1.2f);
			Blood->SpawnDrip(Joint, (Out + FVector(0, 0, 0.35f)).GetSafeNormal() * 260.f * Pulse, FMath::RoundToInt(3.f + 4.f * Pulse));
		}
	}
	for (int32 i = Severed.Num() - 1; i >= 0; --i)
	{
		FSevered& S = Severed[i];
		UPoseableMeshComponent* P = S.Part.Get();
		S.Life -= DeltaTime;
		if (!P || S.Life <= 0.f)
		{
			if (P) { P->DestroyComponent(); }
			Severed.RemoveAtSwap(i);
			continue;
		}
		if (S.bResting)
		{
			continue;
		}
		S.Velocity.Z -= 980.f * DeltaTime;
		const FVector From = S.Pivot;
		FVector To = From + S.Velocity * DeltaTime;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ZSSevered), false);
		if (GetWorld()->SweepSingleByChannel(Hit, From, To, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(S.Radius), Q)
			&& !(Hit.GetActor() && Hit.GetActor()->IsA<APawn>()))
		{
			To = Hit.Location;
			S.Velocity = FMath::GetReflectionVector(S.Velocity, Hit.ImpactNormal) * 0.3f;
			S.Spin *= 0.5f;
			if (Blood && S.Velocity.Size() > 60.f) { Blood->SpawnSplat(Hit.ImpactPoint, Hit.ImpactNormal, FMath::FRandRange(16.f, 28.f)); }
			if (S.Velocity.Size() < 60.f && Hit.ImpactNormal.Z > 0.6f)
			{
				S.bResting = true;
				if (Blood) { Blood->SpawnSplat(Hit.ImpactPoint, Hit.ImpactNormal, FMath::FRandRange(22.f, 36.f)); }
			}
		}
		else if (Blood)
		{
			S.DripTimer -= DeltaTime;
			if (S.DripTimer <= 0.f) { S.DripTimer = 0.05f; Blood->SpawnDrip(To, S.Velocity * 0.2f, 1); }
		}
		S.Pivot = To;
		S.Rotation = FQuat(S.Spin.GetSafeNormal(), FMath::DegreesToRadians(S.Spin.Size() * DeltaTime)) * S.Rotation;
		P->SetWorldTransform(S.Offset * FTransform(S.Rotation, S.Pivot));
	}
	for (int32 i = Gibs.Num() - 1; i >= 0; --i)
	{
		Gibs[i].Life -= DeltaTime;
		UStaticMeshComponent* C = Gibs[i].Comp.Get();
		if (!C || Gibs[i].Life <= 0.f)
		{
			DestroyGib(Gibs[i]);
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
