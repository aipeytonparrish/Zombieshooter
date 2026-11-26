#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "ZSFirstPersonArmsComponent.generated.h"

class USkeletalMesh;

/** Where one hand should be this frame, in world space. Filled by UZSWeaponComponent from the weapon's FZSHandGrip. */
struct FZSArmTarget
{
	FVector Wrist = FVector::ZeroVector;
	FVector FingerDirection = FVector::ForwardVector;
	FVector PalmNormal = FVector::LeftVector;
	FVector ElbowHint = FVector::ZeroVector;
	float FingerCurl = 50.f;
	float IndexCurl = 30.f;
	float ThumbCurl = 20.f;
};

/**
 * First-person arms posed entirely in code: two-bone IK puts each wrist on the weapon's grip, the hand is
 * turned to face the grip, and the fingers curl around it. Works with any arms mesh on the UE5 Manny skeleton
 * (the owner's AAA FPS Gloves packs). Real first-person animations can replace this later - the weapon
 * component only supplies grip targets.
 */
UCLASS(ClassGroup = (ZS), meta = (BlueprintSpawnableComponent))
class ZOMBIESHOOTER_API UZSFirstPersonArmsComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()

public:
	UZSFirstPersonArmsComponent();

	/** Arms mesh to load at BeginPlay (git-ignored pack; arms stay hidden if it isn't present). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Arms")
	TSoftObjectPtr<USkeletalMesh> ArmsMesh;

	/** Where the skeleton's head bone sits relative to the camera (X forward, Y right, Z up). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Arms")
	FVector HeadOffsetFromCamera = FVector(6.f, 0.f, -6.f);

	UFUNCTION(BlueprintCallable, Category = "ZS|Arms")
	void SetArmsMesh(USkeletalMesh* NewMesh);

	bool HasArms() const { return bBonesValid; }

	/** Pose both arms for this frame. */
	void UpdateArms(const FZSArmTarget& Right, const FZSArmTarget& Left);

protected:
	virtual void BeginPlay() override;

private:
	struct FFingerChain
	{
		TArray<int32> Bones;   // 01, 02, 03 (proximal to distal)
		bool bThumb = false;
		bool bIndex = false;
	};

	struct FArmBones
	{
		int32 UpperArm = INDEX_NONE;
		int32 LowerArm = INDEX_NONE;
		int32 Hand = INDEX_NONE;
		TArray<FFingerChain> Fingers;
		FQuat RefHandFrame = FQuat::Identity;   // hand frame (X fingers, Z palm) in reference pose
		FQuat RefHandRotation = FQuat::Identity;
		bool IsValid() const { return UpperArm != INDEX_NONE && LowerArm != INDEX_NONE && Hand != INDEX_NONE; }
	};

	bool InitBones();
	bool InitArm(FArmBones& Arm, const TCHAR* Side, bool bRight);
	void PlaceRelativeToCamera();
	void RecomputeComponentSpace();
	void RecomputeChildren(int32 FromIndex);
	void SetComponentSpace(int32 Bone, const FTransform& CS);
	void SolveArm(const FArmBones& Arm, const FZSArmTarget& InTarget);
	void CurlFingers(const FArmBones& Arm, const FZSArmTarget& Target, const FVector& PalmNormalCS);

	TArray<FTransform> RefLocal;
	TArray<FTransform> Local;
	TArray<FTransform> CS;
	TArray<int32> Parents;

	FArmBones RightArm;
	FArmBones LeftArm;
	int32 HeadBone = INDEX_NONE;
	bool bBonesValid = false;
};
