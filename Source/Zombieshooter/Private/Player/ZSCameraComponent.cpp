#include "Player/ZSCameraComponent.h"
#include "Movement/ZSMovementComponent.h"
#include "Core/ZSGameUserSettings.h"
#include "Camera/CameraTypes.h"

UZSCameraComponent::UZSCameraComponent()
{
	bUsePawnControlRotation = true;
	FieldOfView = 80.f;
}

void UZSCameraComponent::AddLandingImpact(float FallSpeed)
{
	const float Dip = FMath::Min(FMath::Max(0.f, FallSpeed) * LandingDipPerSpeed, LandingDipMax);
	// Push the spring downward; it settles back to zero on its own.
	DipVelocity -= Dip * 12.f;
}

void UZSCameraComponent::UpdateFeel(float DeltaTime, const UZSMovementComponent* Movement)
{
	const UZSGameUserSettings* Settings = UZSGameUserSettings::Get();
	const float EffectScale = FMath::Clamp(Settings->CameraEffectsScale, 0.f, 1.f);

	float TargetRoll = 0.f;
	float TargetFOVKick = 0.f;
	float BobTarget = 0.f;
	float Speed2D = 0.f;
	float SprintAlpha = 0.f;

	if (Movement)
	{
		Speed2D = Movement->Velocity.Size2D();
		const bool bGrounded = Movement->MovementMode == MOVE_Walking;
		const bool bSprinting = Movement->IsSprinting();
		SprintAlpha = bSprinting ? 1.f : 0.f;

		// Head bob only while walking on the ground at speed.
		if (bGrounded && Speed2D > 60.f)
		{
			BobTarget = FMath::Clamp(Speed2D / FMath::Max(Movement->JogSpeed, 1.f), 0.f, 1.f);
		}

		// Tilt
		if (Movement->IsSliding())
		{
			TargetRoll = -SlideTilt;
		}
		else if (Movement->IsWallRunning())
		{
			// Lean away from the wall.
			TargetRoll = Movement->GetWallRunSide() == EZSWallSide::Right ? -WallRunTilt : WallRunTilt;
		}
		else if (const AActor* Owner = GetOwner())
		{
			const float Strafe = FVector::DotProduct(Movement->Velocity, Owner->GetActorRightVector()) / FMath::Max(Movement->SprintSpeed, 1.f);
			TargetRoll = FMath::Clamp(Strafe, -1.f, 1.f) * StrafeTilt;
		}

		// FOV kick (largest active effect wins)
		if (bSprinting)                 TargetFOVKick = FMath::Max(TargetFOVKick, SprintFOVKick);
		if (Movement->IsSliding())      TargetFOVKick = FMath::Max(TargetFOVKick, SlideFOVKick);
		if (Movement->IsWallRunning())  TargetFOVKick = FMath::Max(TargetFOVKick, WallRunFOVKick);
		if (Movement->IsDashing())      TargetFOVKick = FMath::Max(TargetFOVKick, DashFOVKick);
	}

	// ---- Head bob ----
	if (!Settings->bHeadBob)
	{
		BobTarget = 0.f;
	}
	BobWeight = FMath::FInterpTo(BobWeight, BobTarget, DeltaTime, 6.f);
	const float Frequency = FMath::Lerp(BobFrequencyJog, BobFrequencySprint, SprintAlpha);
	BobPhase = FMath::Fmod(BobPhase + DeltaTime * Frequency * 2.f * PI, 2.f * PI);
	const float AmpScale = FMath::Lerp(1.f, BobSprintAmplitudeScale, SprintAlpha) * BobWeight * EffectScale;
	const float BobZ = FMath::Sin(BobPhase * 2.f) * BobVerticalAmplitude * AmpScale;
	const float BobY = FMath::Sin(BobPhase) * BobHorizontalAmplitude * AmpScale;

	// ---- Landing dip (damped spring) ----
	const float SpringAccel = -LandingSpringStiffness * DipOffset - LandingSpringDamping * DipVelocity;
	DipVelocity += SpringAccel * DeltaTime;
	DipOffset += DipVelocity * DeltaTime;
	DipOffset = FMath::Clamp(DipOffset, -LandingDipMax * 1.5f, LandingDipMax);

	FeelOffset = FVector(0.f, BobY, BobZ + DipOffset * EffectScale);

	// ---- Tilt and FOV ----
	CurrentRoll = FMath::FInterpTo(CurrentRoll, TargetRoll * EffectScale, DeltaTime, TiltInterpSpeed);
	CurrentFOVKick = FMath::FInterpTo(CurrentFOVKick, TargetFOVKick * EffectScale, DeltaTime, FOVInterpSpeed);

	SetFieldOfView((Settings->FieldOfView + CurrentFOVKick * (1.f - ADSAlpha)) * ADSFOVMultiplier);
}

void UZSCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	Super::GetCameraView(DeltaTime, DesiredView);

	// Offsets are applied in yaw-only space so bob doesn't swing with pitch.
	const FRotator YawOnly(0.f, DesiredView.Rotation.Yaw, 0.f);
	DesiredView.Location += YawOnly.RotateVector(FVector(0.f, FeelOffset.Y, 0.f)) + FVector(0.f, 0.f, FeelOffset.Z);
	DesiredView.Rotation.Roll += CurrentRoll;
}

FTransform UZSCameraComponent::GetViewFeelLocalTransform() const
{
	const FRotator ViewRotation = GetComponentRotation();
	const FRotator YawOnly(0.f, ViewRotation.Yaw, 0.f);
	const FVector WorldOffset = YawOnly.RotateVector(FVector(0.f, FeelOffset.Y, 0.f)) + FVector(0.f, 0.f, FeelOffset.Z);
	return FTransform(FRotator(0.f, 0.f, CurrentRoll), ViewRotation.UnrotateVector(WorldOffset));
}

void UZSCameraComponent::SetADSFOVMultiplier(float Multiplier, float InADSAlpha)
{
	ADSFOVMultiplier = FMath::Clamp(Multiplier, 0.2f, 1.f);
	ADSAlpha = FMath::Clamp(InADSAlpha, 0.f, 1.f);
}
