#include "Animation/ZSLayeredAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "AnimationRuntime.h"

void FZSLayeredAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	Layers.Reset();
	if (const UZSLayeredAnimInstance* Layered = Cast<UZSLayeredAnimInstance>(InAnimInstance))
	{
		Layered->GatherLayers(Layers);
	}
}

bool FZSLayeredAnimProxy::Evaluate(FPoseContext& Output)
{
	float Accumulated = 0.f;
	for (const FZSAnimLayerSample& L : Layers)
	{
		if (!L.Anim || L.Weight <= KINDA_SMALL_NUMBER || L.bAdditive)
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

	// Additive layers (e.g. fire / reload clips authored as deltas over an idle or aim pose).
	for (const FZSAnimLayerSample& L : Layers)
	{
		if (!L.Anim || L.Weight <= KINDA_SMALL_NUMBER || !L.bAdditive)
		{
			continue;
		}
		FPoseContext Additive(Output.AnimInstanceProxy, true);
		FAnimationPoseData AdditiveData(Additive);
		L.Anim->GetAnimationPose(AdditiveData, FAnimExtractContext(static_cast<double>(L.Time), false, FDeltaTimeRecord(), L.bLoop));
		FAnimationPoseData OutData(Output);
		FAnimationRuntime::AccumulateAdditivePose(OutData, AdditiveData, FMath::Min(L.Weight, 1.f), L.Anim->GetAdditiveAnimType());
		Output.Pose.NormalizeRotations();
	}
	return true;
}

FAnimInstanceProxy* UZSLayeredAnimInstance::CreateAnimInstanceProxy()
{
	return new FZSLayeredAnimProxy(this);
}

void UZSLayeredAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void UZSLayeredAnimInstance::AddLayer(FZSAnimLayerArray& Out, const UAnimSequence* Anim, float Time, float Weight, bool bLoop)
{
	if (Anim && Weight > 0.001f)
	{
		FZSAnimLayerSample& S = Out.AddDefaulted_GetRef();
		S.Anim = Anim;
		const float Length = FMath::Max(Anim->GetPlayLength(), 0.01f);
		S.Time = bLoop ? FMath::Fmod(FMath::Max(Time, 0.f), Length) : FMath::Clamp(Time, 0.f, Length);
		S.Weight = Weight;
		S.bLoop = bLoop;
		S.bAdditive = Anim->IsValidAdditive();
	}
}
