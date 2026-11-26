#pragma once

#include "CoreMinimal.h"
#include "Animation/ZSLayeredAnimInstance.h"
#include "ZSZombieAnimInstance.generated.h"

class UAnimSequence;

/**
 * Code-driven zombie animation: idle / walk / run locomotion blended by speed (play rate matched to the
 * real movement speed so feet don't slide), plus one-shot actions (attack, hit react, rage) that blend in
 * over locomotion and back out. Works directly with the owner's per-zombie animation sets.
 */
UCLASS(Transient, NotBlueprintable)
class ZOMBIESHOOTER_API UZSZombieAnimInstance : public UZSLayeredAnimInstance
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

	virtual void GatherLayers(FZSAnimLayerArray& Out) const override;

protected:
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
