#include "Zombies/ZSZombieAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "AnimationRuntime.h"
#include "GameFramework/Pawn.h"

// ---- Proxy (worker thread) ------------------------------------------------------------------

void FZSZombieAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	Layers.Reset();
	if (const UZSZombieAnimInstance* Zombie = Cast<UZSZombieAnimInstance>(InAnimInstance))
	{
		Zombie->GatherLayers(Layers);
	}
}

bool FZSZombieAnimProxy::Evaluate(FPoseContext& Output)
{
	float Accumulated = 0.f;
	for (const FZSAnimLayerSample& L : Layers)
	{
		if (!L.Anim || L.Weight <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FAnimExtractContext Ctx(static_cast<double>(L.Time), false, FDeltaTimeRecord(), L.bLoop);
		if (Accumulated <= 0.f)
		{
			FAnimationPoseData OutData(Output);
			L.Anim->GetAnimationPose(OutData, Ctx);
			Accumulated = L.Weight;
		}
		else
		{
			FPoseContext Temp(Output);
			FAnimationPoseData TempData(Temp);
			L.Anim->GetAnimationPose(TempData, Ctx);
			FAnimationPoseData OutData(Output);
			const float Total = Accumulated + L.Weight;
			FAnimationRuntime::BlendTwoPosesTogetherInPlace(OutData, TempData, Accumulated / Total);
			Accumulated = Total;
		}
	}

	if (Accumulated <= 0.f)
	{
		Output.ResetToRefPose();
	}
	return true;
}

// ---- Instance (game thread) -----------------------------------------------------------------

FAnimInstanceProxy* UZSZombieAnimInstance::CreateAnimInstanceProxy()
{
	return new FZSZombieAnimProxy(this);
}

void UZSZombieAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void UZSZombieAnimInstance::SetLocomotion(UAnimSequence* InIdle, UAnimSequence* InWalk, UAnimSequence* InRun, float InWalkSpeed, float InRunSpeed)
{
	Idle = InIdle;
	Walk = InWalk;
	Run = InRun;
	WalkSpeed = FMath::Max(InWalkSpeed, 10.f);
	RunSpeed = FMath::Max(InRunSpeed, WalkSpeed + 10.f);
	// Desync crowds.
	IdleTime = FMath::FRand() * 5.f;
	WalkTime = FMath::FRand() * 2.f;
	RunTime = FMath::FRand() * 2.f;
}

float UZSZombieAnimInstance::PlayAction(UAnimSequence* Anim, float PlayRate, float BlendIn, float BlendOut)
{
	if (!Anim || bFrozen)
	{
		return 0.f;
	}
	ActionAnim = Anim;
	ActionTime = 0.f;
	ActionRate = FMath::Max(PlayRate, 0.05f);
	ActionBlendIn = FMath::Max(BlendIn, 0.01f);
	ActionBlendOut = FMath::Max(BlendOut, 0.01f);
	bActionEnding = false;
	return Anim->GetPlayLength() / ActionRate;
}

void UZSZombieAnimInstance::StopAction(float BlendOut)
{
	if (ActionAnim)
	{
		bActionEnding = true;
		ActionBlendOut = FMath::Max(BlendOut, 0.01f);
	}
}

float UZSZombieAnimInstance::GetActionProgress() const
{
	return ActionAnim ? FMath::Clamp(ActionTime / FMath::Max(ActionAnim->GetPlayLength(), 0.01f), 0.f, 1.f) : 0.f;
}

void UZSZombieAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (bFrozen)
	{
		return;
	}

	const APawn* Pawn = TryGetPawnOwner();
	const float Speed = Pawn ? Pawn->GetVelocity().Size2D() : 0.f;

	// Locomotion weights: idle -> walk -> run.
	float TargetIdle = 0.f, TargetWalk = 0.f, TargetRun = 0.f;
	if (Speed < WalkSpeed * 0.35f)
	{
		const float A = FMath::Clamp(Speed / (WalkSpeed * 0.35f), 0.f, 1.f);
		TargetIdle = 1.f - A;
		TargetWalk = A;
	}
	else if (!Run || Speed <= WalkSpeed * 1.2f)
	{
		TargetWalk = 1.f;
	}
	else
	{
		const float A = FMath::Clamp((Speed - WalkSpeed * 1.2f) / FMath::Max(RunSpeed * 0.7f - WalkSpeed * 1.2f, 1.f), 0.f, 1.f);
		TargetWalk = 1.f - A;
		TargetRun = A;
	}
	if (!Walk) { TargetIdle += TargetWalk; TargetWalk = 0.f; }
	if (!Run) { TargetWalk += TargetRun; TargetRun = 0.f; }

	const float BlendSpeed = 6.f;
	IdleWeight = FMath::FInterpTo(IdleWeight, TargetIdle, DeltaSeconds, BlendSpeed);
	WalkWeight = FMath::FInterpTo(WalkWeight, TargetWalk, DeltaSeconds, BlendSpeed);
	RunWeight = FMath::FInterpTo(RunWeight, TargetRun, DeltaSeconds, BlendSpeed);

	// Play rates matched to ground speed so feet don't slide.
	const float WalkRate = FMath::Clamp(Speed / WalkSpeed, 0.6f, 1.6f);
	const float RunRate = FMath::Clamp(Speed / RunSpeed, 0.7f, 1.45f);
	IdleTime += DeltaSeconds;
	WalkTime += DeltaSeconds * WalkRate;
	RunTime += DeltaSeconds * RunRate;

	// One-shot action.
	if (ActionAnim)
	{
		ActionTime += DeltaSeconds * ActionRate;
		const float Length = ActionAnim->GetPlayLength();
		const float Remaining = (Length - ActionTime) / ActionRate;
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

void UZSZombieAnimInstance::GatherLayers(TArray<FZSAnimLayerSample, TInlineAllocator<5>>& Out) const
{
	const float LocoScale = 1.f - ActionWeight;
	const float Sum = FMath::Max(IdleWeight + WalkWeight + RunWeight, KINDA_SMALL_NUMBER);

	auto Add = [&Out](const UAnimSequence* Anim, float Time, float Weight, bool bLoop)
	{
		if (Anim && Weight > 0.001f)
		{
			FZSAnimLayerSample& S = Out.AddDefaulted_GetRef();
			S.Anim = Anim;
			S.Time = bLoop ? FMath::Fmod(Time, FMath::Max(Anim->GetPlayLength(), 0.01f)) : FMath::Min(Time, Anim->GetPlayLength());
			S.Weight = Weight;
			S.bLoop = bLoop;
		}
	};

	Add(Idle, IdleTime, LocoScale * IdleWeight / Sum, true);
	Add(Walk, WalkTime, LocoScale * WalkWeight / Sum, true);
	Add(Run, RunTime, LocoScale * RunWeight / Sum, true);
	Add(ActionAnim, ActionTime, ActionWeight, false);
}
