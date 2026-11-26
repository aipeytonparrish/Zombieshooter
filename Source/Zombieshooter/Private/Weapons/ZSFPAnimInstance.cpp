#include "Weapons/ZSFPAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void UZSFPAnimInstance::SetLoops(UAnimSequence* InIdle, UAnimSequence* InWalk, UAnimSequence* InSprint, UAnimSequence* InAimIdle, UAnimSequence* InAimWalk)
{
	Idle = InIdle;
	Walk = InWalk;
	Sprint = InSprint;
	AimIdle = InAimIdle ? InAimIdle : InIdle;
	AimWalk = InAimWalk ? InAimWalk : AimIdle.Get();
}

void UZSFPAnimInstance::SetLocomotionState(float InSpeed01, float InSprintAlpha, float InADSAlpha)
{
	Speed01 = FMath::Clamp(InSpeed01, 0.f, 1.5f);
	SprintAlpha = FMath::Clamp(InSprintAlpha, 0.f, 1.f);
	ADSAlpha = FMath::Clamp(InADSAlpha, 0.f, 1.f);
}

float UZSFPAnimInstance::PlayAction(UAnimSequence* Anim, float PlayRate, float BlendIn, float BlendOut, const UAnimMontage* NotifySource, const TArray<FZSActionSound>* TimedSounds)
{
	if (!Anim)
	{
		return 0.f;
	}
	// Restarting the same action (e.g. every shot of full auto) keeps the current weight so it doesn't pop.
	if (ActionAnim != Anim)
	{
		ActionWeight = FMath::Min(ActionWeight, 0.5f);
	}
	ActionAnim = Anim;
	ActionTime = 0.f;
	ActionRate = FMath::Max(PlayRate, 0.05f);
	ActionBlendIn = FMath::Max(BlendIn, 0.01f);
	ActionBlendOut = FMath::Max(BlendOut, 0.01f);
	bActionEnding = false;
	ActionNotifySource = NotifySource;
	ActionTimedSounds.Reset();
	if (TimedSounds)
	{
		ActionTimedSounds = *TimedSounds;
	}
	// Anything scheduled at the very start.
	FireSounds(-1.f, 0.f);
	return Anim->GetPlayLength() / ActionRate;
}

void UZSFPAnimInstance::StopAction(float BlendOut)
{
	if (ActionAnim)
	{
		bActionEnding = true;
		ActionBlendOut = FMath::Max(BlendOut, 0.01f);
		ActionNotifySource = nullptr;
		ActionTimedSounds.Reset();
	}
}

void UZSFPAnimInstance::FireSounds(float FromTime, float ToTime)
{
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	auto Play = [Mesh, this](USoundBase* Sound, float Volume, float Pitch)
	{
		if (!Sound)
		{
			return;
		}
		if (Mesh)
		{
			UGameplayStatics::SpawnSoundAttached(Sound, Mesh, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, Volume, Pitch);
		}
		else
		{
			UGameplayStatics::PlaySound2D(this, Sound, Volume, Pitch);
		}
	};

	if (ActionNotifySource)
	{
		for (const FAnimNotifyEvent& Event : ActionNotifySource->Notifies)
		{
			const float T = Event.GetTriggerTime();
			if (T > FromTime && T <= ToTime)
			{
				if (const UAnimNotify_PlaySound* PlaySound = Cast<UAnimNotify_PlaySound>(Event.Notify))
				{
					Play(PlaySound->Sound, PlaySound->VolumeMultiplier, PlaySound->PitchMultiplier);
				}
			}
		}
	}

	if (ActionAnim && ActionTimedSounds.Num() > 0)
	{
		const float Length = FMath::Max(ActionAnim->GetPlayLength(), 0.01f);
		for (const FZSActionSound& S : ActionTimedSounds)
		{
			const float T = S.Fraction * Length;
			if (T > FromTime && T <= ToTime)
			{
				Play(S.Sound, 1.f, 1.f);
			}
		}
	}
}

void UZSFPAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	IdleTime += DeltaSeconds;
	WalkTime += DeltaSeconds * FMath::Clamp(Speed01, 0.6f, 1.3f);
	SprintTime += DeltaSeconds;

	if (ActionAnim)
	{
		const float Previous = ActionTime;
		ActionTime += DeltaSeconds * ActionRate;
		if (!bActionEnding)
		{
			FireSounds(Previous, ActionTime);
		}

		const float Remaining = (ActionAnim->GetPlayLength() - ActionTime) / ActionRate;
		if (!bActionEnding && Remaining <= ActionBlendOut)
		{
			bActionEnding = true;
		}
		if (bActionEnding)
		{
			ActionWeight = FMath::Max(0.f, ActionWeight - DeltaSeconds / ActionBlendOut);
			if (ActionWeight <= 0.f)
			{
				ActionAnim = nullptr;
			}
		}
		else
		{
			ActionWeight = FMath::Min(1.f, ActionWeight + DeltaSeconds / ActionBlendIn);
		}
	}
	else
	{
		ActionWeight = 0.f;
	}
}

void UZSFPAnimInstance::GatherLayers(FZSAnimLayerArray& Out) const
{
	// A full-pose action replaces the base while it plays; an additive one rides on top of it.
	const bool bAdditiveAction = ActionAnim && ActionAnim->IsValidAdditive();
	const float Base = bAdditiveAction ? 1.f : 1.f - ActionWeight;
	const float Move = FMath::Clamp(Speed01, 0.f, 1.f);
	const float Hip = (1.f - ADSAlpha);
	const float NotSprint = 1.f - SprintAlpha;

	AddLayer(Out, Idle, IdleTime, Base * Hip * NotSprint * (1.f - Move), true);
	AddLayer(Out, Walk, WalkTime, Base * Hip * NotSprint * Move, true);
	AddLayer(Out, Sprint, SprintTime, Base * Hip * SprintAlpha, true);
	AddLayer(Out, AimIdle, IdleTime, Base * ADSAlpha * (1.f - Move), true);
	AddLayer(Out, AimWalk, WalkTime, Base * ADSAlpha * Move, true);
	AddLayer(Out, ActionAnim, ActionTime, ActionWeight, false);
}
