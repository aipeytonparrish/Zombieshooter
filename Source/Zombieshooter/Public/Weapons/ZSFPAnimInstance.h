#pragma once

#include "CoreMinimal.h"
#include "Animation/ZSLayeredAnimInstance.h"
#include "ZSFPAnimInstance.generated.h"

class UAnimSequence;
class UAnimMontage;
class USoundBase;

/** A sound to play at a point (0..1) of an action. */
struct FZSActionSound
{
	float Fraction = 0.f;
	TObjectPtr<USoundBase> Sound;
};

/**
 * Code-driven first-person animation for the arms rig and for animated weapon meshes:
 *  - base loops: idle / walk / sprint (hip) and idle / walk (aimed), blended by speed, sprint and ADS
 *  - one action at a time on top (fire, reload, equip, holster) with blend in / out
 *  - sounds during the action: PlaySound notifies harvested from a montage, plus explicit timed sounds
 */
UCLASS(Transient, NotBlueprintable)
class ZOMBIESHOOTER_API UZSFPAnimInstance : public UZSLayeredAnimInstance
{
	GENERATED_BODY()

public:
	void SetLoops(UAnimSequence* InIdle, UAnimSequence* InWalk, UAnimSequence* InSprint, UAnimSequence* InAimIdle, UAnimSequence* InAimWalk);

	/** Called every frame by the weapon component. Speed01: 0 = still, 1 = jogging. */
	void SetLocomotionState(float InSpeed01, float InSprintAlpha, float InADSAlpha);

	/**
	 * Play a one-shot. Returns its duration in seconds. NotifySource: a montage whose PlaySound notifies should fire
	 * in sync (e.g. the weapon's reload montage). Sounds play attached to SoundAttach (or at the owner).
	 */
	float PlayAction(UAnimSequence* Anim, float PlayRate = 1.f, float BlendIn = 0.08f, float BlendOut = 0.2f,
		const UAnimMontage* NotifySource = nullptr, const TArray<FZSActionSound>* TimedSounds = nullptr);

	void StopAction(float BlendOut = 0.15f);

	bool IsPlayingAction() const { return ActionAnim != nullptr && !bActionEnding; }

	virtual void GatherLayers(FZSAnimLayerArray& Out) const override;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void FireSounds(float FromTime, float ToTime);

	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Idle;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Sprint;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> AimIdle;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> AimWalk;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> ActionAnim;
	UPROPERTY(Transient) TObjectPtr<const UAnimMontage> ActionNotifySource;

	TArray<FZSActionSound> ActionTimedSounds;

	float Speed01 = 0.f, SprintAlpha = 0.f, ADSAlpha = 0.f;
	float IdleTime = 0.f, WalkTime = 0.f, SprintTime = 0.f;

	float ActionTime = 0.f;
	float ActionRate = 1.f;
	float ActionBlendIn = 0.08f;
	float ActionBlendOut = 0.2f;
	float ActionWeight = 0.f;
	bool bActionEnding = false;
};
