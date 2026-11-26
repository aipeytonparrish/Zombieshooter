#include "Movement/ZSMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Zombieshooter.h"

namespace ZSMovement
{
	static FVector Flatten(const FVector& V)
	{
		return FVector(V.X, V.Y, 0.f);
	}

	static const TCHAR* ModeName(EZSCustomMovementMode Mode)
	{
		switch (Mode)
		{
		case EZSCustomMovementMode::Slide:   return TEXT("Slide");
		case EZSCustomMovementMode::WallRun: return TEXT("WallRun");
		case EZSCustomMovementMode::Mantle:  return TEXT("Mantle");
		default:                             return TEXT("Custom");
		}
	}
}

UZSMovementComponent::UZSMovementComponent()
{
	// Base character tuning for a fast, responsive FPS.
	NavAgentProps.bCanCrouch = true;
	NavAgentProps.bCanJump = true;
	bCanWalkOffLedgesWhenCrouching = true;
	bOrientRotationToMovement = false;

	MaxWalkSpeed = JogSpeed;
	MaxWalkSpeedCrouched = CrouchSpeed;
	MaxAcceleration = 2600.f;
	BrakingDecelerationWalking = 2400.f;
	GroundFriction = 8.f;
	BrakingDecelerationFalling = 0.f;
	FallingLateralFriction = 0.f;
	AirControl = ZSAirControl;
	AirControlBoostMultiplier = 1.f;
	AirControlBoostVelocityThreshold = 0.f;

	GravityScale = 1.5f;
	JumpZVelocity = 650.f;
	MaxStepHeight = 45.f;
	SetWalkableFloorAngle(46.f);

	SetCrouchedHalfHeight(52.f);
	PerchRadiusThreshold = 0.f;
}

void UZSMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
	AirControl = ZSAirControl;
	MaxWalkSpeed = JogSpeed;
	MaxWalkSpeedCrouched = CrouchSpeed;
	AirDashesRemaining = MaxAirDashes;
}

// =============================================================================================
// Queries
// =============================================================================================

bool UZSMovementComponent::IsCustomMode(EZSCustomMovementMode Mode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(Mode);
}

bool UZSMovementComponent::IsSprinting() const
{
	return bWantsToSprint
		&& MovementMode == MOVE_Walking
		&& !IsCrouching()
		&& IsMovingForward(SprintMaxAngle);
}

bool UZSMovementComponent::IsMovingForward(float MaxAngleDeg) const
{
	if (!UpdatedComponent)
	{
		return false;
	}
	const FVector Input = ZSMovement::Flatten(Acceleration).GetSafeNormal();
	if (Input.IsNearlyZero())
	{
		return false;
	}
	const FVector Forward = ZSMovement::Flatten(UpdatedComponent->GetForwardVector()).GetSafeNormal();
	return FVector::DotProduct(Input, Forward) >= FMath::Cos(FMath::DegreesToRadians(MaxAngleDeg));
}

FVector UZSMovementComponent::GetInputDirection2D() const
{
	return ZSMovement::Flatten(Acceleration).GetSafeNormal();
}

float UZSMovementComponent::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

float UZSMovementComponent::GetCapsuleRadius() const
{
	return CharacterOwner ? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.f;
}

float UZSMovementComponent::GetCapsuleHalfHeight() const
{
	return CharacterOwner ? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
}

bool UZSMovementComponent::CanCoyoteJump() const
{
	return IsFalling() && !bLeftGroundByJumping && TimeSinceLeftGround <= CoyoteTime;
}

FString UZSMovementComponent::GetDebugStateString() const
{
	FString Mode;
	switch (MovementMode)
	{
	case MOVE_Walking: Mode = IsCrouching() ? TEXT("Crouch") : (IsSprinting() ? TEXT("Sprint") : TEXT("Walk")); break;
	case MOVE_Falling: Mode = TEXT("Air"); break;
	case MOVE_Custom:  Mode = ZSMovement::ModeName(static_cast<EZSCustomMovementMode>(CustomMovementMode)); break;
	default:           Mode = TEXT("Other"); break;
	}

	const int32 Jumps = CharacterOwner ? CharacterOwner->JumpCurrentCount : 0;
	const int32 MaxJumps = CharacterOwner ? CharacterOwner->JumpMaxCount : 0;

	return FString::Printf(TEXT("%s%s | %4.0f cm/s (vz %4.0f) | jumps %d/%d | dash cd %.1fs air %d"),
		*Mode,
		IsDashing() ? TEXT(" +Dash") : TEXT(""),
		Velocity.Size2D(), Velocity.Z,
		Jumps, MaxJumps,
		DashCooldownRemaining, AirDashesRemaining);
}

// =============================================================================================
// Speeds
// =============================================================================================

float UZSMovementComponent::GetMaxSpeed() const
{
	if (IsDashing())
	{
		return DashSpeed;
	}

	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
		if (IsCrouching())
		{
			return CrouchSpeed;
		}
		return IsSprinting() ? SprintSpeed : JogSpeed;

	case MOVE_Falling:
		return JogSpeed;

	case MOVE_Custom:
		switch (static_cast<EZSCustomMovementMode>(CustomMovementMode))
		{
		case EZSCustomMovementMode::Slide:   return SlideMaxSpeed;
		case EZSCustomMovementMode::WallRun: return WallRunSpeed;
		case EZSCustomMovementMode::Mantle:  return MantleExitSpeed;
		default: break;
		}
		break;

	default:
		break;
	}
	return Super::GetMaxSpeed();
}

float UZSMovementComponent::GetMaxAcceleration() const
{
	return IsSprinting() ? SprintAcceleration : Super::GetMaxAcceleration();
}

float UZSMovementComponent::GetMaxBrakingDeceleration() const
{
	if (IsDashing() || MovementMode == MOVE_Custom)
	{
		return 0.f;
	}
	return Super::GetMaxBrakingDeceleration();
}

bool UZSMovementComponent::IsMovingOnGround() const
{
	// Sliding counts as grounded so crouch, jump and floor logic treat it like walking.
	return Super::IsMovingOnGround() || IsSliding();
}

bool UZSMovementComponent::CanCrouchInCurrentState() const
{
	if (!CanEverCrouch() || !UpdatedComponent || UpdatedComponent->IsSimulatingPhysics())
	{
		return false;
	}
	return IsFalling() || IsMovingOnGround() || IsMantling();
}

// =============================================================================================
// Jumping (ground jump, coyote jump, double jump, slide-hop, wall jump, mantle-on-jump)
// =============================================================================================

bool UZSMovementComponent::CanAttemptJump() const
{
	// Unlike the engine default, crouching does not block jumping: you stand up as you jump.
	return IsJumpAllowed() && !IsMantling() && (IsMovingOnGround() || IsFalling() || IsWallRunning());
}

bool UZSMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	if (!CharacterOwner)
	{
		return false;
	}

	// A jump press in front of a climbable ledge mantles instead (even if jumps are used up).
	if (!bReplayingMoves && TryStartMantle())
	{
		return false;
	}

	if (!CharacterOwner->CanJump())
	{
		return false;
	}

	if (IsWallRunning())
	{
		// Wall jump: push away from the wall, keep running speed along it, bias toward stick input.
		const FVector Along = ZSMovement::Flatten(Velocity).GetSafeNormal();
		const float AlongSpeed = FMath::Max(Velocity.Size2D(), WallRunSpeed * 0.8f);
		FVector NewVelocity = Along * AlongSpeed + WallRunNormal * WallJumpOffSpeed;

		const FVector InputDir = GetInputDirection2D();
		if (!InputDir.IsNearlyZero())
		{
			const float Speed2D = NewVelocity.Size2D();
			NewVelocity = FMath::Lerp(NewVelocity, InputDir * Speed2D, 0.35f);
		}
		NewVelocity.Z = WallJumpUpSpeed;
		Velocity = NewVelocity;

		bLeftGroundByJumping = true;
		ExitWallRun();
		return true;
	}

	const bool bGroundJump = IsMovingOnGround() || CanCoyoteJump();

	if (IsSliding())
	{
		// Slide-hop: keep all slide momentum and add a little.
		const FVector Dir = ZSMovement::Flatten(Velocity).GetSafeNormal();
		Velocity += Dir * SlideHopBoost;
		const float Speed2D = FMath::Min(Velocity.Size2D(), SlideMaxSpeed);
		Velocity = Dir * Speed2D + FVector::UpVector * Velocity.Z;
	}

	if (bGroundJump)
	{
		Velocity.Z = FMath::Max<FVector::FReal>(Velocity.Z, JumpZVelocity);
	}
	else
	{
		// Double jump: fresh upward speed and partial redirect toward the stick direction.
		const FVector InputDir = GetInputDirection2D();
		if (!InputDir.IsNearlyZero())
		{
			const float Speed2D = FMath::Max(Velocity.Size2D(), JogSpeed * 0.75f);
			const FVector Current = ZSMovement::Flatten(Velocity);
			const FVector Target = InputDir * Speed2D;
			const FVector Blended = FMath::Lerp(Current, Target, DoubleJumpRedirect);
			Velocity.X = Blended.X;
			Velocity.Y = Blended.Y;
		}
		Velocity.Z = DoubleJumpZVelocity;
		OnDoubleJump.Broadcast();
	}

	// Jumping always stands you up (unless the ceiling prevents it).
	bWantsToCrouch = false;
	bWantsToSlide = false;
	bLeftGroundByJumping = true;
	SetMovementMode(MOVE_Falling);
	return true;
}

// =============================================================================================
// Crouch / slide input
// =============================================================================================

void UZSMovementComponent::HandleCrouchPressed(bool bToggleMode)
{
	if (IsSliding())
	{
		// Pressing crouch during a slide cancels it and stands up.
		bWantsToCrouch = false;
		bWantsToSlide = false;
		return;
	}

	const bool bFastEnough = GetHorizontalSpeed() >= SlideMinEnterSpeed;

	if (IsMovingOnGround() && bFastEnough)
	{
		bWantsToSlide = true;
		return;
	}

	if (IsFalling() && bFastEnough)
	{
		// Queue a slide for the moment we land (press again to cancel the queue).
		bWantsToSlide = !bWantsToSlide;
		return;
	}

	bWantsToCrouch = bToggleMode ? !bWantsToCrouch : true;
	if (bWantsToCrouch)
	{
		bWantsToSprint = false;
	}
}

void UZSMovementComponent::HandleCrouchReleased(bool bToggleMode)
{
	if (!bToggleMode)
	{
		bWantsToCrouch = false;
		bWantsToSlide = false;
	}
}

void UZSMovementComponent::Crouch(bool bClientSimulation)
{
	bLastCrouchMaintainedBase = bCrouchMaintainsBaseLocation;
	Super::Crouch(bClientSimulation);
}

void UZSMovementComponent::UnCrouch(bool bClientSimulation)
{
	bLastCrouchMaintainedBase = bCrouchMaintainsBaseLocation;
	Super::UnCrouch(bClientSimulation);
}

// =============================================================================================
// Per-update state machine
// =============================================================================================

void UZSMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// Timers
	SlideBoostCooldownRemaining = FMath::Max(0.f, SlideBoostCooldownRemaining - DeltaSeconds);
	DashCooldownRemaining = FMath::Max(0.f, DashCooldownRemaining - DeltaSeconds);
	TimeSinceLeftGround = IsMovingOnGround() ? 0.f : TimeSinceLeftGround + DeltaSeconds;

	// Slide start (from sprint on the ground)
	if (bWantsToSlide)
	{
		if (IsMovingOnGround() && !IsSliding())
		{
			if (CanSlide())
			{
				EnterSlide();
			}
			bWantsToSlide = false;
		}
		else if (!IsFalling())
		{
			bWantsToSlide = false;
		}
	}

	// Slide cancel (crouch pressed again / released in hold mode / jumped)
	if (IsSliding() && !bWantsToCrouch)
	{
		ExitSlide(true);
	}

	// Dash
	if (bWantsToDash)
	{
		TryStartDash();
		bWantsToDash = false;
	}
	TickDash(DeltaSeconds);

	// Auto-mantle while airborne and pushing into a ledge
	if (bAutoMantleInAir && IsFalling() && !IsDashing() && IsMovingForward(45.f))
	{
		TryStartMantle();
	}

	// Wall-run start
	if (IsFalling() && !IsDashing())
	{
		TryStartWallRun();
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UZSMovementComponent::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);

	// Drop stale sprint intent when it can't apply (e.g. crouched); toggle-sprint is re-requested by input.
	if (bWantsToSprint && IsCrouching() && !IsSliding())
	{
		bWantsToSprint = false;
	}
}

void UZSMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	const bool bLeftCustom = PreviousMovementMode == MOVE_Custom
		&& (MovementMode != MOVE_Custom || CustomMovementMode != PreviousCustomMode);
	if (bLeftCustom)
	{
		const EZSCustomMovementMode OldMode = static_cast<EZSCustomMovementMode>(PreviousCustomMode);
		if (OldMode == EZSCustomMovementMode::WallRun)
		{
			WallRunSide = EZSWallSide::None;
		}
		OnCustomModeEnded.Broadcast(OldMode);
	}

	if (MovementMode == MOVE_Custom && (PreviousMovementMode != MOVE_Custom || PreviousCustomMode != CustomMovementMode))
	{
		OnCustomModeStarted.Broadcast(static_cast<EZSCustomMovementMode>(CustomMovementMode));
	}

	if (MovementMode == MOVE_Walking)
	{
		AirDashesRemaining = MaxAirDashes;
		LastWallRunNormal = FVector::ZeroVector;
		bLeftGroundByJumping = false;
	}

	if (IsWallRunning())
	{
		AirDashesRemaining = MaxAirDashes;
	}

	const bool bWasGrounded = PreviousMovementMode == MOVE_Walking
		|| (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == static_cast<uint8>(EZSCustomMovementMode::Slide));
	if (MovementMode == MOVE_Falling && bWasGrounded)
	{
		TimeSinceLeftGround = 0.f;
		bLeftGroundByJumping = CharacterOwner && CharacterOwner->bPressedJump;
	}
}

void UZSMovementComponent::SetPostLandedPhysics(const FHitResult& Hit)
{
	Super::SetPostLandedPhysics(Hit);

	// Land straight into a queued slide so no momentum is lost to a frame of ground friction.
	if (bWantsToSlide && IsMovingOnGround() && CanSlide())
	{
		EnterSlide();
		bWantsToSlide = false;
	}
}

void UZSMovementComponent::SetCustomMode(EZSCustomMovementMode Mode)
{
	SetMovementMode(MOVE_Custom, static_cast<uint8>(Mode));
}

void UZSMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	switch (static_cast<EZSCustomMovementMode>(CustomMovementMode))
	{
	case EZSCustomMovementMode::Slide:   PhysSlide(DeltaTime, Iterations); break;
	case EZSCustomMovementMode::WallRun: PhysWallRun(DeltaTime, Iterations); break;
	case EZSCustomMovementMode::Mantle:  PhysMantle(DeltaTime, Iterations); break;
	default:
		UE_LOG(LogZS, Warning, TEXT("Unknown custom movement mode %d"), CustomMovementMode);
		SetMovementMode(MOVE_Walking);
		break;
	}

	Super::PhysCustom(DeltaTime, Iterations);
}

// =============================================================================================
// Slide
// =============================================================================================

bool UZSMovementComponent::CanSlide() const
{
	return CharacterOwner
		&& IsMovingOnGround()
		&& !IsSliding()
		&& GetHorizontalSpeed() >= SlideMinEnterSpeed;
}

void UZSMovementComponent::EnterSlide()
{
	bWantsToCrouch = true;
	if (!IsCrouching())
	{
		// Crouch while still walking so the capsule keeps its feet on the floor.
		Crouch(false);
	}

	FVector Dir = ZSMovement::Flatten(Velocity).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = ZSMovement::Flatten(UpdatedComponent->GetForwardVector()).GetSafeNormal();
	}

	if (SlideBoostCooldownRemaining <= 0.f)
	{
		Velocity += Dir * SlideEnterBoost;
		SlideBoostCooldownRemaining = SlideBoostCooldown;
	}
	Velocity = Velocity.GetClampedToMaxSize(SlideMaxSpeed);

	SetCustomMode(EZSCustomMovementMode::Slide);
}

void UZSMovementComponent::ExitSlide(bool bStandUp)
{
	if (bStandUp)
	{
		bWantsToCrouch = false;
	}
	SetMovementMode(MOVE_Walking);
}

bool UZSMovementComponent::FindSlideSurface(FHitResult& OutHit) const
{
	if (!UpdatedComponent || !CharacterOwner)
	{
		return false;
	}

	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start + FVector::DownVector * (GetCapsuleHalfHeight() + 30.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSSlideSurface), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(Params, ResponseParams);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, UpdatedComponent->GetCollisionObjectType(), Params, ResponseParams);
	return bHit && IsWalkable(OutHit);
}

void UZSMovementComponent::PhysSlide(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	FHitResult Surface;
	if (!FindSlideSurface(Surface))
	{
		// Slid off a ledge: keep momentum into the air, still crouched (slide can resume on landing).
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	if (GetHorizontalSpeed() < SlideMinSpeed)
	{
		ExitSlide(false);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	const FVector Normal = Surface.ImpactNormal;

	// Gravity along the slope: downhill speeds up, uphill slows down.
	Velocity += FVector::VectorPlaneProject(FVector::DownVector * SlideGravityForce, Normal) * DeltaTime;

	// Steering: only the sideways part of the stick counts.
	const FVector Right = FVector::CrossProduct(FVector::UpVector, ZSMovement::Flatten(Velocity).GetSafeNormal());
	const float Steer = FVector::DotProduct(GetInputDirection2D(), Right);
	Velocity += Right * Steer * SlideSteerAcceleration * DeltaTime;

	// Friction and cap
	Velocity -= Velocity * FMath::Min(SlideFriction * DeltaTime, 1.f);
	Velocity = FVector::VectorPlaneProject(Velocity, Normal).GetClampedToMaxSize(SlideMaxSpeed);

	// Move
	Iterations++;
	bJustTeleported = false;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FQuat Rotation = UpdatedComponent->GetComponentQuat();
	const FVector Delta = Velocity * DeltaTime;

	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, Rotation, true, Hit);
	if (Hit.Time < 1.f)
	{
		HandleImpact(Hit, DeltaTime, Delta);
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}

	// Keep glued to the ground over small crests.
	FHitResult After;
	if (FindSlideSurface(After))
	{
		const float Gap = After.Distance - GetCapsuleHalfHeight();
		if (Gap > 1.f)
		{
			FHitResult SnapHit;
			SafeMoveUpdatedComponent(FVector::DownVector * Gap, Rotation, true, SnapHit);
		}
	}

	if (!bJustTeleported)
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}
}

// =============================================================================================
// Wall-run
// =============================================================================================

bool UZSMovementComponent::IsWallRunnable(const FHitResult& Hit) const
{
	if (!Hit.bBlockingHit || IsWalkable(Hit))
	{
		return false;
	}
	if (Cast<APawn>(Hit.GetActor()))
	{
		return false;
	}
	const float MaxZ = FMath::Sin(FMath::DegreesToRadians(WallRunMaxWallAngle));
	return FMath::Abs(Hit.ImpactNormal.Z) <= MaxZ;
}

bool UZSMovementComponent::FindRunnableWall(EZSWallSide& OutSide, FHitResult& OutHit) const
{
	if (!UpdatedComponent || !CharacterOwner)
	{
		return false;
	}

	const FVector VelDir = ZSMovement::Flatten(Velocity).GetSafeNormal();
	if (VelDir.IsNearlyZero())
	{
		return false;
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, VelDir);
	const FVector Start = UpdatedComponent->GetComponentLocation();
	const float Reach = GetCapsuleRadius() + WallRunTraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSWallRun), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(Params, ResponseParams);
	const ECollisionChannel Channel = UpdatedComponent->GetCollisionObjectType();

	float BestDistance = TNumericLimits<float>::Max();
	bool bFound = false;

	for (const EZSWallSide Side : { EZSWallSide::Right, EZSWallSide::Left })
	{
		const FVector SideDir = Side == EZSWallSide::Right ? Right : -Right;
		const FVector Dirs[] = { SideDir, (SideDir + VelDir).GetSafeNormal() };
		for (const FVector& Dir : Dirs)
		{
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * Reach, Channel, Params, ResponseParams)
				&& IsWallRunnable(Hit)
				&& Hit.Distance < BestDistance)
			{
				BestDistance = Hit.Distance;
				OutHit = Hit;
				OutSide = Side;
				bFound = true;
			}
		}
	}
	return bFound;
}

bool UZSMovementComponent::TryStartWallRun()
{
	if (!IsFalling() || !CharacterOwner)
	{
		return false;
	}
	if (GetHorizontalSpeed() < WallRunMinEnterSpeed || Velocity.Z < -700.f)
	{
		return false;
	}

	// Player must be pushing roughly along their direction of travel.
	const FVector VelDir = ZSMovement::Flatten(Velocity).GetSafeNormal();
	if (FVector::DotProduct(GetInputDirection2D(), VelDir) < 0.3f)
	{
		return false;
	}

	// Must be high enough above the ground.
	{
		const FVector Feet = UpdatedComponent->GetComponentLocation() - FVector::UpVector * GetCapsuleHalfHeight();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSWallRunHeight), false, CharacterOwner);
		FHitResult GroundHit;
		if (GetWorld()->LineTraceSingleByChannel(GroundHit, Feet, Feet - FVector::UpVector * WallRunMinHeight, ECC_Visibility, Params))
		{
			return false;
		}
	}

	EZSWallSide Side = EZSWallSide::None;
	FHitResult WallHit;
	if (!FindRunnableWall(Side, WallHit))
	{
		return false;
	}

	const FVector Normal = ZSMovement::Flatten(WallHit.ImpactNormal).GetSafeNormal();

	// Running straight into a wall is a mantle/stop, not a wall-run.
	if (FVector::DotProduct(VelDir, -Normal) > 0.8f)
	{
		return false;
	}

	// Same-wall rule: no re-running the wall you just left until landing or cooldown.
	if (!LastWallRunNormal.IsNearlyZero()
		&& FVector::DotProduct(LastWallRunNormal, Normal) > 0.9f
		&& GetWorldTime() - LastWallRunEndTime < WallRunSameWallCooldown)
	{
		return false;
	}

	WallRunSide = Side;
	WallRunNormal = Normal;
	WallRunElapsed = 0.f;
	Velocity.Z = FMath::Clamp<FVector::FReal>(Velocity.Z, -50.f, WallRunMaxEnterZVelocity);
	SetCustomMode(EZSCustomMovementMode::WallRun);
	return true;
}

void UZSMovementComponent::ExitWallRun()
{
	LastWallRunNormal = WallRunNormal;
	LastWallRunEndTime = GetWorldTime();
	WallRunSide = EZSWallSide::None;
	SetMovementMode(MOVE_Falling);
}

void UZSMovementComponent::PhysWallRun(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	WallRunElapsed += DeltaTime;

	// Re-acquire the wall on the side we are running.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSWallRunTick), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(Params, ResponseParams);
	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start - WallRunNormal * (GetCapsuleRadius() + WallRunTraceDistance * 1.5f);

	FHitResult WallHit;
	const bool bHasWall = GetWorld()->LineTraceSingleByChannel(WallHit, Start, End, UpdatedComponent->GetCollisionObjectType(), Params, ResponseParams)
		&& IsWallRunnable(WallHit);

	const FVector InputDir = GetInputDirection2D();
	const FVector VelDir = ZSMovement::Flatten(Velocity).GetSafeNormal();
	const bool bPushingAlong = FVector::DotProduct(InputDir, VelDir) > 0.1f;

	if (!bHasWall || !bPushingAlong || WallRunElapsed > WallRunMaxDuration || GetHorizontalSpeed() < WallRunMinSpeed)
	{
		ExitWallRun();
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	WallRunNormal = ZSMovement::Flatten(WallHit.ImpactNormal).GetSafeNormal();

	// Horizontal: along the wall, easing toward wall-run speed.
	FVector Along = ZSMovement::Flatten(FVector::VectorPlaneProject(Velocity, WallRunNormal)).GetSafeNormal();
	if (Along.IsNearlyZero())
	{
		ExitWallRun();
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}
	const float Speed = FMath::FInterpTo(GetHorizontalSpeed(), WallRunSpeed, DeltaTime, 4.f);

	// Vertical: gravity ramps in over the run so it arcs downward.
	const float Alpha = FMath::Clamp(WallRunElapsed / FMath::Max(WallRunMaxDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float GravScale = FMath::Lerp(WallRunGravityScaleStart, WallRunGravityScaleEnd, Alpha);
	const float NewZ = Velocity.Z + GetGravityZ() * GravScale * DeltaTime;

	Velocity = Along * Speed + FVector::UpVector * NewZ;

	Iterations++;
	bJustTeleported = false;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FQuat Rotation = UpdatedComponent->GetComponentQuat();
	const FVector Delta = Velocity * DeltaTime - WallRunNormal * 2.f; // slight stick into the wall

	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, Rotation, true, Hit);
	if (Hit.Time < 1.f)
	{
		HandleImpact(Hit, DeltaTime, Delta);
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}

	if (!bJustTeleported)
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}

	// Close to the floor while descending: drop out and let falling physics land us.
	if (Velocity.Z <= 0.f)
	{
		const FVector Feet = UpdatedComponent->GetComponentLocation() - FVector::UpVector * GetCapsuleHalfHeight();
		FHitResult FloorHit;
		if (GetWorld()->LineTraceSingleByChannel(FloorHit, Feet, Feet - FVector::UpVector * 10.f, UpdatedComponent->GetCollisionObjectType(), Params, ResponseParams)
			&& IsWalkable(FloorHit))
		{
			ExitWallRun();
		}
	}
}

// =============================================================================================
// Mantle
// =============================================================================================

bool UZSMovementComponent::TryStartMantle()
{
	if (!CharacterOwner || !UpdatedComponent || IsMantling())
	{
		return false;
	}
	if (!(IsMovingOnGround() || IsFalling() || IsWallRunning()))
	{
		return false;
	}

	const FVector Forward = ZSMovement::Flatten(UpdatedComponent->GetForwardVector()).GetSafeNormal();

	// Don't mantle if the player is pulling away.
	const FVector InputDir = GetInputDirection2D();
	if (!InputDir.IsNearlyZero() && FVector::DotProduct(InputDir, Forward) < 0.2f)
	{
		return false;
	}

	UWorld* World = GetWorld();
	const float Radius = GetCapsuleRadius();
	const float HalfHeight = GetCapsuleHalfHeight();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const float FeetZ = Location.Z - HalfHeight;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSMantle), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(Params, ResponseParams);
	const ECollisionChannel Channel = UpdatedComponent->GetCollisionObjectType();

	// 1) Find a wall in front, between the min and max mantle heights.
	FHitResult WallHit;
	bool bFoundWall = false;
	const float Reach = Radius + MantleReach;
	const int32 NumSamples = 4;
	for (int32 i = 0; i < NumSamples && !bFoundWall; ++i)
	{
		const float SampleHeight = FMath::Lerp(MantleMinHeight, MantleMaxHeight - 10.f, static_cast<float>(i) / (NumSamples - 1));
		const FVector Start(Location.X, Location.Y, FeetZ + SampleHeight);
		if (World->LineTraceSingleByChannel(WallHit, Start, Start + Forward * Reach, Channel, Params, ResponseParams)
			&& !IsWalkable(WallHit)
			&& !Cast<APawn>(WallHit.GetActor()))
		{
			bFoundWall = true;
		}
	}
	if (!bFoundWall)
	{
		return false;
	}

	const FVector WallNormal = ZSMovement::Flatten(WallHit.ImpactNormal).GetSafeNormal();
	if (WallNormal.IsNearlyZero() || FVector::DotProduct(Forward, -WallNormal) < 0.5f)
	{
		return false; // too glancing
	}
	const FVector IntoWall = -WallNormal;

	// 2) Find the top surface just beyond the wall face.
	const FVector TopXY = WallHit.ImpactPoint + IntoWall * (Radius + 4.f);
	const FVector DownStart(TopXY.X, TopXY.Y, FeetZ + MantleMaxHeight + 5.f);
	const FVector DownEnd(TopXY.X, TopXY.Y, FeetZ + MantleMinHeight - 5.f);
	FHitResult TopHit;
	if (!World->LineTraceSingleByChannel(TopHit, DownStart, DownEnd, Channel, Params, ResponseParams)
		|| !IsWalkable(TopHit)
		|| TopHit.bStartPenetrating)
	{
		return false;
	}

	const float LedgeHeight = TopHit.ImpactPoint.Z - FeetZ;
	if (LedgeHeight < MantleMinHeight || LedgeHeight > MantleMaxHeight)
	{
		return false;
	}
	if (IsMovingOnGround() && LedgeHeight <= MaxStepHeight)
	{
		return false;
	}

	// 3) Clearance: standing capsule first, crouched capsule as fallback.
	const ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
	const float StandHalfHeight = DefaultCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float CrouchHalfHeight = GetCrouchedHalfHeight();

	auto Fits = [&](const FVector& Center, float TestHalfHeight)
	{
		return !World->OverlapBlockingTestByChannel(Center, FQuat::Identity, Channel,
			FCollisionShape::MakeCapsule(Radius, TestHalfHeight), Params, ResponseParams);
	};

	const float TopZ = TopHit.ImpactPoint.Z;
	const bool bStandFits = Fits(FVector(TopXY.X, TopXY.Y, TopZ + StandHalfHeight + 2.f), StandHalfHeight);
	const bool bCrouchFits = bStandFits || Fits(FVector(TopXY.X, TopXY.Y, TopZ + CrouchHalfHeight + 2.f), CrouchHalfHeight);
	if (!bCrouchFits)
	{
		return false;
	}

	if (!bStandFits && !IsCrouching())
	{
		bWantsToCrouch = true;
		Crouch(false);
	}

	// Vertical path above us must be clear too (low ceilings).
	const float CurrentHalfHeight = GetCapsuleHalfHeight();
	const FVector Target(TopXY.X, TopXY.Y, TopZ + CurrentHalfHeight + 2.f);
	const FVector RiseTop(Location.X, Location.Y, Target.Z);
	if (!Fits(RiseTop, CurrentHalfHeight))
	{
		return false;
	}

	if (IsWallRunning())
	{
		ExitWallRun();
	}
	if (IsSliding())
	{
		bWantsToCrouch = !bStandFits;
	}

	MantleStartLocation = UpdatedComponent->GetComponentLocation();
	MantleTargetLocation = Target;
	MantleForward = IntoWall;
	MantleElapsed = 0.f;
	const float HeightAlpha = FMath::GetRangePct(MantleMinHeight, MantleMaxHeight, LedgeHeight);
	MantleDuration = FMath::Lerp(MantleDurationLow, MantleDurationHigh, FMath::Clamp(HeightAlpha, 0.f, 1.f));
	DashTimeRemaining = 0.f;
	Velocity = FVector::ZeroVector;
	SetCustomMode(EZSCustomMovementMode::Mantle);
	return true;
}

void UZSMovementComponent::PhysMantle(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	MantleElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(MantleElapsed / FMath::Max(MantleDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);

	// Rise first (ease out), then pull forward over the lip (ease in/out).
	const float VertAlpha = FMath::InterpEaseOut(0.f, 1.f, FMath::Clamp(Alpha / 0.6f, 0.f, 1.f), 2.f);
	const float HorizAlpha = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp((Alpha - 0.45f) / 0.55f, 0.f, 1.f), 2.f);

	FVector Desired;
	Desired.X = FMath::Lerp(MantleStartLocation.X, MantleTargetLocation.X, HorizAlpha);
	Desired.Y = FMath::Lerp(MantleStartLocation.Y, MantleTargetLocation.Y, HorizAlpha);
	Desired.Z = FMath::Lerp(MantleStartLocation.Z, MantleTargetLocation.Z, VertAlpha);

	const FVector Delta = Desired - UpdatedComponent->GetComponentLocation();
	FHitResult Hit;
	// Clearance was validated when the mantle started, so move without sweeping to avoid snagging the lip.
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), false, Hit);
	Velocity = Delta / DeltaTime;

	if (Alpha >= 1.f)
	{
		SetMovementMode(MOVE_Walking);
		Velocity = MantleForward * MantleExitSpeed;
	}
}

// =============================================================================================
// Dash
// =============================================================================================

bool UZSMovementComponent::TryStartDash()
{
	if (DashCooldownRemaining > 0.f || IsMantling() || IsDashing() || !UpdatedComponent)
	{
		return false;
	}

	const bool bInAir = IsFalling() || IsWallRunning();
	if (bInAir && AirDashesRemaining <= 0)
	{
		return false;
	}

	FVector Dir = GetInputDirection2D();
	if (Dir.IsNearlyZero())
	{
		Dir = ZSMovement::Flatten(UpdatedComponent->GetForwardVector()).GetSafeNormal();
	}

	if (IsWallRunning())
	{
		ExitWallRun();
	}
	if (IsSliding())
	{
		ExitSlide(true);
	}

	DashDirection = Dir;
	DashTimeRemaining = DashDuration;
	DashCooldownRemaining = DashCooldown;
	if (bInAir)
	{
		--AirDashesRemaining;
	}

	Velocity = Dir * DashSpeed + FVector::UpVector * (bInAir ? 0.f : Velocity.Z);
	OnDashStarted.Broadcast();
	return true;
}

void UZSMovementComponent::TickDash(float DeltaSeconds)
{
	if (DashTimeRemaining <= 0.f)
	{
		return;
	}

	DashTimeRemaining -= DeltaSeconds;
	if (DashTimeRemaining > 0.f)
	{
		// Hold dash velocity for its full duration (cancels friction and gravity in the air).
		const float Z = IsFalling() ? 0.f : Velocity.Z;
		Velocity = DashDirection * DashSpeed + FVector::UpVector * Z;
	}
	else
	{
		DashTimeRemaining = 0.f;
		const FVector Horizontal = ZSMovement::Flatten(Velocity).GetClampedToMaxSize(DashExitSpeed);
		Velocity = Horizontal + FVector::UpVector * Velocity.Z;
	}
}

// =============================================================================================
// Networking-ready saved moves
// =============================================================================================

void UZSMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToDash = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}

FNetworkPredictionData_Client* UZSMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UZSMovementComponent* MutableThis = const_cast<UZSMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FZSNetworkPredictionData_Client(*this);
	}
	return ClientPredictionData;
}

void FZSSavedMove::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = 0;
	bSavedWantsToDash = 0;
}

uint8 FZSSavedMove::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedWantsToSprint)
	{
		Result |= FLAG_Custom_0;
	}
	if (bSavedWantsToDash)
	{
		Result |= FLAG_Custom_1;
	}
	return Result;
}

bool FZSSavedMove::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FZSSavedMove* Other = static_cast<const FZSSavedMove*>(NewMove.Get());
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint || bSavedWantsToDash != Other->bSavedWantsToDash)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FZSSavedMove::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	if (const UZSMovementComponent* Move = Cast<UZSMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedWantsToSprint = Move->bWantsToSprint;
		bSavedWantsToDash = Move->bWantsToDash;
	}
}

void FZSSavedMove::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (UZSMovementComponent* Move = Cast<UZSMovementComponent>(C->GetCharacterMovement()))
	{
		Move->bWantsToSprint = bSavedWantsToSprint;
		Move->bWantsToDash = bSavedWantsToDash;
	}
}

FZSNetworkPredictionData_Client::FZSNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FZSNetworkPredictionData_Client::AllocateNewMove()
{
	return FSavedMovePtr(new FZSSavedMove());
}
