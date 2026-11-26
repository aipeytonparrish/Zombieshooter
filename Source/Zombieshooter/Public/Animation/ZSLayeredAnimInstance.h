#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "ZSLayeredAnimInstance.generated.h"

class UAnimSequence;

/** One sampled animation and its blend weight, copied to the worker thread each frame. */
struct FZSAnimLayerSample
{
	const UAnimSequence* Anim = nullptr;
	float Time = 0.f;
	float Weight = 0.f;
	bool bLoop = true;
	/** Additive clips are applied on top of the blended base pose instead of blending with it. */
	bool bAdditive = false;
};

using FZSAnimLayerArray = TArray<FZSAnimLayerSample, TInlineAllocator<8>>;

/** Worker-thread side: samples the layers and blends them by weight. No Anim Blueprint graph involved. */
struct FZSLayeredAnimProxy : public FAnimInstanceProxy
{
	FZSLayeredAnimProxy() = default;
	explicit FZSLayeredAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	FZSAnimLayerArray Layers;
};

/**
 * Base for code-driven animation: subclasses decide each frame which sequences play at what time and weight
 * (GatherLayers, game thread); the proxy samples and blends them. Used by zombies, first-person arms and weapons,
 * so no Anim Blueprints are needed for any of them.
 */
UCLASS(Abstract, Transient, NotBlueprintable)
class ZOMBIESHOOTER_API UZSLayeredAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Fill Out with this frame's layers (weights need not sum to 1; they are normalised). */
	virtual void GatherLayers(FZSAnimLayerArray& Out) const {}

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	static void AddLayer(FZSAnimLayerArray& Out, const UAnimSequence* Anim, float Time, float Weight, bool bLoop);
};
