#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "ZSZombieAnimInstance.generated.h"

class UAnimSequence;

/** One sampled animation and its blend weight, copied to the worker thread each frame. */
struct FZSAnimLayerSample
{
	const UAnimSequence* Anim = nullptr;
	float Time = 0.f;
	float Weight = 0.f;
	bool bLoop = true;
};

/** Worker-thread side: samples the layers and blends them. No Anim Blueprint graph is involved. */
struct FZSZombieAnimProxy : public FAnimInstanceProxy
{
	FZSZombieAnimProxy() = default;
	explicit FZSZombieAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	TArray<FZSAnimLayerSample, TInlineAllocator<5>> Layers;
};

/**
 * Code-driven zombie animation: idle / walk / run locomotion blended by speed (play rate matched to the
 * real movement speed so feet don't slide), plus one-shot actions (attack, hit react, rage) that blend in
 * over locomotion and back out. Works directly with the owner's per-zombie animation sets.
 */
UCLASS(Transient, NotBlueprintable)
class ZOMBIESHOOTER_API UZSZombieAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Locomotion set. WalkSpeed / RunSpeed are the ground speeds (cm/s) the clips were authored at. */
	void SetLocomotion(UAnimSequence* InIdle, UAnimSequence* InWalk, UAnimSequence* InRun, float InWalkSpeed, float InRunSpeed);

	/** Play a one-shot over locomotion. Returns its duration in seconds (0 if Anim is null). */
	float PlayAction(UAnimSequence* Anim, float PlayRate = 1.f, float BlendIn = 0.15f, float BlendOut = 0.25f);

	void StopAction(float BlendOut = 0.2f);

	bool IsPlayingAction() const { return ActionAnim != nullptr && !bActionEnding; }

	/** 0..1 progress of the current action (0 when none). */
	float GetActionProgress() const;

	/** Freeze everything (used the moment ragdoll takes over). */
	void Freeze() { bFrozen = true; }

	// Read by the proxy on the game thread in PreUpdate.
	void GatherLayers(TArray<FZSAnimLayerSample, TInlineAllocator<5>>& Out) const;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Idle;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Walk;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Run;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ActionAnim;

	float WalkSpeed = 120.f;
	float RunSpeed = 400.f;

	float IdleTime = 0.f, WalkTime = 0.f, RunTime = 0.f;
	float IdleWeight = 1.f, WalkWeight = 0.f, RunWeight = 0.f;

	float ActionTime = 0.f;
	float ActionRate = 1.f;
	float ActionBlendIn = 0.15f;
	float ActionBlendOut = 0.25f;
	float ActionWeight = 0.f;
	bool bActionEnding = false;
	bool bFrozen = false;
};
