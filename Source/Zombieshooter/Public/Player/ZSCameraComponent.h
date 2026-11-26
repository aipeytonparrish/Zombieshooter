#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "ZSCameraComponent.generated.h"

class UZSMovementComponent;

/**
 * First-person camera with movement feel layered on top of the control rotation:
 * head bob, slide / wall-run tilt, strafe lean, landing dip, and FOV kicks for sprint / slide / dash / wall-run.
 *
 * Effects are computed in UpdateFeel() (called from the character's Tick) and applied in GetCameraView(),
 * which runs after the controller has updated its rotation for the frame, so mouse aim never lags.
 * Base FOV and the head-bob toggle come from UZSGameUserSettings.
 *
 * Milestone 9 adds the over-the-shoulder mode on top of this component.
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class ZOMBIESHOOTER_API UZSCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	UZSCameraComponent();

	/** Advance all camera-feel effects. */
	void UpdateFeel(float DeltaTime, const UZSMovementComponent* Movement);

	/** Kick the landing dip. FallSpeed is the (positive) downward speed at impact. */
	void AddLandingImpact(float FallSpeed);

	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

	/** The bob / roll offset GetCameraView applies to the view, expressed in this component's local space.
	 *  First-person view models apply it too so they stay locked to the view. */
	FTransform GetViewFeelLocalTransform() const;

	/** ADS zoom: final FOV = (settings FOV + kicks) * Multiplier. FOV kicks fade out while aiming. */
	void SetADSFOVMultiplier(float Multiplier, float ADSAlpha);

	// ---- Tuning ----

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|HeadBob", meta = (ForceUnits = "cm"))
	float BobVerticalAmplitude = 1.1f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|HeadBob", meta = (ForceUnits = "cm"))
	float BobHorizontalAmplitude = 0.7f;

	/** Bob cycles per second at jog speed and at sprint speed. */
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|HeadBob")
	float BobFrequencyJog = 1.7f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|HeadBob")
	float BobFrequencySprint = 2.3f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|HeadBob")
	float BobSprintAmplitudeScale = 1.6f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|Tilt", meta = (ForceUnits = "deg"))
	float SlideTilt = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|Tilt", meta = (ForceUnits = "deg"))
	float WallRunTilt = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|Tilt", meta = (ForceUnits = "deg"))
	float StrafeTilt = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|Tilt")
	float TiltInterpSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|Landing", meta = (ForceUnits = "cm"))
	float LandingDipMax = 9.f;

	/** cm of dip per cm/s of fall speed. */
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|Landing")
	float LandingDipPerSpeed = 0.008f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|Landing")
	float LandingSpringStiffness = 170.f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|Landing")
	float LandingSpringDamping = 18.f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|FOV", meta = (ForceUnits = "deg"))
	float SprintFOVKick = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|FOV", meta = (ForceUnits = "deg"))
	float SlideFOVKick = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|FOV", meta = (ForceUnits = "deg"))
	float WallRunFOVKick = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|FOV", meta = (ForceUnits = "deg"))
	float DashFOVKick = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera|FOV")
	float FOVInterpSpeed = 9.f;

private:
	float BobPhase = 0.f;
	float BobWeight = 0.f;
	float CurrentRoll = 0.f;
	float CurrentFOVKick = 0.f;
	float DipOffset = 0.f;
	float DipVelocity = 0.f;

	/** Camera-space additive offset: Y = right, Z = up. */
	FVector FeelOffset = FVector::ZeroVector;

	float ADSFOVMultiplier = 1.f;
	float ADSAlpha = 0.f;
};
