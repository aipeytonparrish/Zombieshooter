#include "Player/ZSFirstPersonArmsComponent.h"
#include "Engine/SkeletalMesh.h"
#include "TwoBoneIK.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Zombieshooter.h"

static TAutoConsoleVariable<int32> CVarZSArmsDebug(TEXT("zs.ArmsDebug"), 0, TEXT("Draw desired (thin) vs actual (thick) hand directions: green = fingers, blue = palm."));

UZSFirstPersonArmsComponent::UZSFirstPersonArmsComponent()
{
	SetOnlyOwnerSee(true);
	SetCastShadow(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bCastDynamicShadow = false;
	// Hands sit right in front of the camera; never let bounds culling hide them.
	bUseAttachParentBound = false;
	SetBoundsScale(4.f);

	ArmsMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/AAA_FPS_Gloves/Modules/SK_Arms_01.SK_Arms_01")));
}

void UZSFirstPersonArmsComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetSkinnedAsset() && !ArmsMesh.IsNull())
	{
		if (USkeletalMesh* Mesh = ArmsMesh.LoadSynchronous())
		{
			SetArmsMesh(Mesh);
		}
		else
		{
			UE_LOG(LogZS, Warning, TEXT("First-person arms mesh %s not found - arms hidden (restore the AAA FPS Gloves pack, see Docs/ThirdPartyAssets.md)."),
				*ArmsMesh.ToString());
		}
	}
	SetVisibility(bBonesValid);
}

void UZSFirstPersonArmsComponent::SetArmsMesh(USkeletalMesh* NewMesh)
{
	SetSkinnedAssetAndUpdate(NewMesh, true);
	bBonesValid = InitBones();
	if (bBonesValid)
	{
		PlaceRelativeToCamera();
	}
	else if (NewMesh)
	{
		UE_LOG(LogZS, Warning, TEXT("Arms mesh %s is not on the UE5 Manny skeleton (missing arm/hand/finger bones) - arms hidden."), *NewMesh->GetName());
	}
	SetVisibility(bBonesValid);
}

// ---- Setup ----------------------------------------------------------------------------------

bool UZSFirstPersonArmsComponent::InitBones()
{
	const USkinnedAsset* Asset = GetSkinnedAsset();
	if (!Asset)
	{
		return false;
	}

	const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
	const int32 Num = Ref.GetNum();
	RefLocal = Ref.GetRefBonePose();
	Parents.SetNum(Num);
	for (int32 i = 0; i < Num; ++i)
	{
		Parents[i] = Ref.GetParentIndex(i);
	}
	Local = RefLocal;
	RecomputeComponentSpace();

	HeadBone = GetBoneIndex(TEXT("head"));
	const bool bRight = InitArm(RightArm, TEXT("r"), true);
	const bool bLeft = InitArm(LeftArm, TEXT("l"), false);
	return bRight && bLeft && BoneSpaceTransforms.Num() == Num;
}

bool UZSFirstPersonArmsComponent::InitArm(FArmBones& Arm, const TCHAR* Side, bool bRight)
{
	auto Bone = [this, Side](const TCHAR* Base) { return GetBoneIndex(FName(FString::Printf(TEXT("%s_%s"), Base, Side))); };

	Arm = FArmBones();
	Arm.UpperArm = Bone(TEXT("upperarm"));
	Arm.LowerArm = Bone(TEXT("lowerarm"));
	Arm.Hand = Bone(TEXT("hand"));
	if (!Arm.IsValid())
	{
		return false;
	}

	const TCHAR* FingerNames[] = { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
	for (const TCHAR* Finger : FingerNames)
	{
		FFingerChain Chain;
		Chain.bThumb = FCString::Strcmp(Finger, TEXT("thumb")) == 0;
		Chain.bIndex = FCString::Strcmp(Finger, TEXT("index")) == 0;
		for (int32 j = 1; j <= 3; ++j)
		{
			const int32 Idx = Bone(*FString::Printf(TEXT("%s_0%d"), Finger, j));
			if (Idx != INDEX_NONE)
			{
				Chain.Bones.Add(Idx);
			}
		}
		if (Chain.Bones.Num() > 0)
		{
			Arm.Fingers.Add(Chain);
		}
	}

	const int32 Middle = Bone(TEXT("middle_01"));
	const int32 Index = Bone(TEXT("index_01"));
	const int32 Pinky = Bone(TEXT("pinky_01"));
	if (Middle == INDEX_NONE || Index == INDEX_NONE || Pinky == INDEX_NONE)
	{
		return false;
	}

	// Reference hand frame: X = wrist -> knuckles, Z = palm normal. In the A-pose palms face down / inward.
	const FVector HandPos = CS[Arm.Hand].GetLocation();
	const FVector F = (CS[Middle].GetLocation() - HandPos).GetSafeNormal();
	const FVector Across = (CS[Index].GetLocation() - CS[Pinky].GetLocation()).GetSafeNormal();
	FVector N = FVector::CrossProduct(F, Across).GetSafeNormal();
	// Manny faces +Y; the character's right is +X, so "inward" for the right hand is -X.
	const FVector Inward = bRight ? FVector(-1.f, 0.f, 0.f) : FVector(1.f, 0.f, 0.f);
	if (-N.Z + FVector::DotProduct(N, Inward) < 0.f)
	{
		N = -N;
	}

	Arm.RefHandFrame = FRotationMatrix::MakeFromXZ(F, N).ToQuat();
	Arm.RefHandRotation = CS[Arm.Hand].GetRotation();
	return true;
}

void UZSFirstPersonArmsComponent::PlaceRelativeToCamera()
{
	// The Manny skeleton faces +Y; turn it to face the camera's +X.
	const FRotator MeshRotation(0.f, -90.f, 0.f);
	SetRelativeRotation(MeshRotation);
	const FVector HeadCS = HeadBone != INDEX_NONE ? CS[HeadBone].GetLocation() : FVector(0.f, 0.f, 160.f);
	SetRelativeLocation(HeadOffsetFromCamera - MeshRotation.RotateVector(HeadCS));
}

// ---- Pose math ------------------------------------------------------------------------------

void UZSFirstPersonArmsComponent::RecomputeComponentSpace()
{
	CS.SetNum(Local.Num());
	for (int32 i = 0; i < Local.Num(); ++i)
	{
		CS[i] = Parents[i] == INDEX_NONE ? Local[i] : Local[i] * CS[Parents[i]];
	}
}

void UZSFirstPersonArmsComponent::RecomputeChildren(int32 FromIndex)
{
	// Bones are stored parent-first, so a forward pass fixes every descendant of FromIndex.
	for (int32 i = FromIndex + 1; i < Local.Num(); ++i)
	{
		if (Parents[i] != INDEX_NONE)
		{
			CS[i] = Local[i] * CS[Parents[i]];
		}
	}
}

void UZSFirstPersonArmsComponent::SetComponentSpace(int32 Bone, const FTransform& NewCS)
{
	CS[Bone] = NewCS;
	const int32 Parent = Parents[Bone];
	Local[Bone] = Parent == INDEX_NONE ? NewCS : NewCS.GetRelativeTransform(CS[Parent]);
	Local[Bone].SetScale3D(RefLocal[Bone].GetScale3D());
	RecomputeChildren(Bone);
}

void UZSFirstPersonArmsComponent::UpdateArms(const FZSArmTarget& Right, const FZSArmTarget& Left)
{
	if (!bBonesValid)
	{
		return;
	}

	Local = RefLocal;
	RecomputeComponentSpace();

	SolveArm(RightArm, Right);
	SolveArm(LeftArm, Left);

	BoneSpaceTransforms = Local;
	MarkRefreshTransformDirty();

}

void UZSFirstPersonArmsComponent::SolveArm(const FArmBones& Arm, const FZSArmTarget& InTarget)
{
	FZSArmTarget Target = InTarget;
	if (CVarZSArmsDebug.GetValueOnGameThread() == 2)
	{
		const FTransform& Cam = GetAttachParent() ? GetAttachParent()->GetComponentTransform() : FTransform::Identity;
		Target.FingerDirection = Cam.TransformVectorNoScale(FVector(1.f, 0.f, 0.f));
		Target.PalmNormal = Cam.TransformVectorNoScale(FVector(0.f, 0.f, -1.f));
		Target.FingerCurl = Target.IndexCurl = Target.ThumbCurl = 0.f;
	}
	const FTransform& ToCS = GetComponentTransform();
	const FVector T = ToCS.InverseTransformPosition(Target.Wrist);
	const FVector Pole = ToCS.InverseTransformPosition(Target.ElbowHint);
	const FVector Fd = ToCS.InverseTransformVector(Target.FingerDirection).GetSafeNormal();
	FVector Nd = ToCS.InverseTransformVector(Target.PalmNormal).GetSafeNormal();
	Nd = (Nd - Fd * FVector::DotProduct(Nd, Fd)).GetSafeNormal();

	const FVector S = CS[Arm.UpperArm].GetLocation();
	const FVector E = CS[Arm.LowerArm].GetLocation();
	const FVector H = CS[Arm.Hand].GetLocation();

	FVector OutE, OutH;
	// Slight stretch is allowed so long guns stay reachable without pulling the shoulders forward too far.
	AnimationCore::SolveTwoBoneIK(S, E, H, Pole, T, OutE, OutH, true, 0.98, 1.12);

	// Upper arm: swing the elbow onto its solved position.
	const FQuat DeltaU = FQuat::FindBetweenNormals((E - S).GetSafeNormal(), (OutE - S).GetSafeNormal());
	const FQuat UpperRot = DeltaU * CS[Arm.UpperArm].GetRotation();
	SetComponentSpace(Arm.UpperArm, FTransform(UpperRot, S));

	// Forearm: swing the wrist onto its solved position.
	const FVector E2 = CS[Arm.LowerArm].GetLocation();
	const FVector H2 = CS[Arm.Hand].GetLocation();
	const FQuat DeltaL = FQuat::FindBetweenNormals((H2 - E2).GetSafeNormal(), (OutH - OutE).GetSafeNormal());
	SetComponentSpace(Arm.LowerArm, FTransform(DeltaL * CS[Arm.LowerArm].GetRotation(), OutE));

	// Hand: match the grip frame.
	const FQuat Desired = FRotationMatrix::MakeFromXZ(Fd, Nd).ToQuat();
	const FQuat HandRot = Desired * Arm.RefHandFrame.Inverse() * Arm.RefHandRotation;
	SetComponentSpace(Arm.Hand, FTransform(HandRot, OutH));

	CurlFingers(Arm, Target, Nd);

	if (CVarZSArmsDebug.GetValueOnGameThread() > 0 && Arm.Fingers.Num() > 2)
	{
		const FTransform& W = GetComponentTransform();
		const FVector Wrist = W.TransformPosition(CS[Arm.Hand].GetLocation());
		const FQuat ActualFrame = CS[Arm.Hand].GetRotation() * Arm.RefHandRotation.Inverse() * Arm.RefHandFrame;
		const FVector ActualF = W.TransformVectorNoScale(ActualFrame.GetAxisX());
		const FVector ActualN = W.TransformVectorNoScale(ActualFrame.GetAxisZ());
		DrawDebugLine(GetWorld(), Wrist, Wrist + Target.FingerDirection * 12.f, FColor::Green, false, -1.f, SDPG_Foreground, 0.3f);
		DrawDebugLine(GetWorld(), Wrist, Wrist + Target.PalmNormal * 12.f, FColor::Blue, false, -1.f, SDPG_Foreground, 0.3f);
		DrawDebugLine(GetWorld(), Wrist, Wrist + ActualF * 9.f, FColor::Green, false, -1.f, SDPG_Foreground, 1.2f);
		DrawDebugLine(GetWorld(), Wrist, Wrist + ActualN * 9.f, FColor::Blue, false, -1.f, SDPG_Foreground, 1.2f);
		const FVector Knuckle = W.TransformPosition(CS[Arm.Fingers[2].Bones[0]].GetLocation());
		DrawDebugLine(GetWorld(), Wrist, Knuckle, FColor::Red, false, -1.f, SDPG_Foreground, 1.2f);
		if (GFrameCounter % 90 == 0)
		{
			UE_LOG(LogZS, Warning, TEXT("ArmsDebug %s: fingers=%d wantF=%s gotF=%s geomF=%s wantN=%s gotN=%s"),
				&Arm == &RightArm ? TEXT("R") : TEXT("L"), Arm.Fingers.Num(),
				*Target.FingerDirection.ToCompactString(), *ActualF.ToCompactString(), *(Knuckle - Wrist).GetSafeNormal().ToCompactString(),
				*Target.PalmNormal.ToCompactString(), *ActualN.ToCompactString());
		}
	}
}

void UZSFirstPersonArmsComponent::CurlFingers(const FArmBones& Arm, const FZSArmTarget& Target, const FVector& PalmNormalCS)
{
	for (const FFingerChain& Chain : Arm.Fingers)
	{
		const float Curl = Chain.bThumb ? Target.ThumbCurl : (Chain.bIndex ? Target.IndexCurl : Target.FingerCurl);
		if (FMath::IsNearlyZero(Curl))
		{
			continue;
		}
		for (int32 j = 0; j < Chain.Bones.Num(); ++j)
		{
			const int32 B = Chain.Bones[j];
			const FVector P = CS[B].GetLocation();
			FVector Dir;
			if (j + 1 < Chain.Bones.Num())
			{
				Dir = CS[Chain.Bones[j + 1]].GetLocation() - P;
			}
			else
			{
				Dir = P - CS[Parents[B]].GetLocation();
			}
			Dir = Dir.GetSafeNormal();

			// Unreal is left-handed: rotating Dir about (Dir x PalmNormal) by a NEGATIVE angle bends it toward the palm.
			const FVector Axis = FVector::CrossProduct(Dir, PalmNormalCS).GetSafeNormal();
			if (Axis.IsNearlyZero())
			{
				continue;
			}
			const float JointCurl = Chain.bThumb ? Curl * (j == 0 ? 0.4f : 1.f) : Curl * (j == 2 ? 0.8f : 1.f);
			const FQuat Bend(Axis, -FMath::DegreesToRadians(JointCurl));
			SetComponentSpace(B, FTransform(Bend * CS[B].GetRotation(), P));
		}
	}
}
