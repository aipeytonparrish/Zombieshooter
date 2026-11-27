#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ZSMovementComponent.generated.h"



/** Custom movement modes used with MOVE_Custom. */
UENUM(BlueprintType)
enum class EZSCustomMovementMode : uint8
{
	None    UMETA(Hidden),
	Slide   UMETA(DisplayName = "Slide"),
	WallRun UMETA(DisplayName = "Wall Run"),
	Mantle  UMETA(DisplayName = "Mantle"),
	MAX     UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EZSWallSide : uint8
{
	None,
	Left,
	Right
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZSMovementEventSignature, EZSCustomMovementMode, Mode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FZSMovementSimpleEventSignature);

/**
 * Full movement kit: sprint, crouch, slide (+ slide-hop), mantle, double jump, wall-run (+ wall jump), dash.
 *
 * Single-player game, but written in the replicated-ready CharacterMovement style: player intent
 * (sprint, dash) travels through saved-move compressed flags and all state changes happen inside the
 * movement update, never directly from input handlers.
 *
 * Every tuning value is an EditDefaultsOnly property so a Blueprint child (or the defaults here)
 * can be tuned without touching code.
 */
UCLASS()
class ZOMBIESHOOTER_API UZSMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	friend class FZSSavedMove;

public:
	UZSMovementComponent();

	// ---- Input-facing API (called by the character) ------------------------------------------

	/** Multiplier on ground jog / crouch speed (aiming down sights slows the player). */
	void SetExternalSpeedScale(float Scale) { ExternalSpeedScale = FMath::Clamp(Scale, 0.1f, 2.f); }

	void SetSprintInput(bool bWantsSprint) { bWantsToSprint = bWantsSprint; }
	bool GetSprintInput() const { return bWantsToSprint; }

	/** Queues a dash for the next movement update (respects cooldown / air charges). */
	void RequestDash() { bWantsToDash = true; }

	/** Crouch button pressed. While sprinting fast enough this starts a slide instead of a crouch. */
	void HandleCrouchPressed(bool bToggleMode);
	void HandleCrouchReleased(bool bToggleMode);

	// ---- State queries -----------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "ZS|Movement")
	bool IsCustomMode(EZSCustomMovementMode Mode) const;

	UFUNCTION(BlueprintPure, Category = "ZS|Movement")
	bool IsSliding() const { return IsCustomMode(EZSCustomMovementMode::Slide); }

	UFUNCTION(BlueprintPure, Category = "ZS|Movement")
	bool IsWallRunning() const { return IsCustomMode(EZSCustomMovementMode::WallRun); }

	UFUNCTION(BlueprintPure, Category = "ZS|Movement")
	bool IsMantling() const { return IsCustomMode(EZSCustomMovementMode::Mantle); }

	/** True while actually sprinting (button held/toggled AND moving forward on the ground). */
	UFUNCTION(BlueprintPure, Category = "ZS|Movement")
	bool IsSprinting() const;

	UFUNCTION(BlueprintPure, Category = "ZS|Movement")
	bool IsDashing() const { return DashTimeRemaining > 0.f; }

	UFUNCTION(BlueprintPure, Category = "ZS|Movement")
	EZSWallSide GetWallRunSide() const { return WallRunSide; }

	UFUNCTION(BlueprintPure, Category = "ZS|Movement")
	float GetDashCooldownRemaining() const { return DashCooldownRemaining; }

	UFUNCTION(BlueprintPure, Category = "ZS|Movement")
	int32 GetAirDashesRemaining() const { return AirDashesRemaining; }

	/** True for a short window after walking (not jumping) off a ledge: the ground jump still works. */
	bool CanCoyoteJump() const;

	/** Short human-readable state for the debug overlay. */
	FString GetDebugStateString() const;

	// ---- Events (camera feel, audio, animation hook into these) ------------------------------

	UPROPERTY(BlueprintAssignable, Category = "ZS|Movement")
	FZSMovementEventSignature OnCustomModeStarted;

	UPROPERTY(BlueprintAssignable, Category = "ZS|Movement")
	FZSMovementEventSignature OnCustomModeEnded;

	UPROPERTY(BlueprintAssignable, Category = "ZS|Movement")
	FZSMovementSimpleEventSignature OnDashStarted;

	UPROPERTY(BlueprintAssignable, Category = "ZS|Movement")
	FZSMovementSimpleEventSignature OnDoubleJump;

	/** Set by the crouch system so the camera can smooth capsule height changes correctly. */
	bool DidLastCrouchMaintainBase() const { return bLastCrouchMaintainedBase; }

	// ---- UCharacterMovementComponent overrides -----------------------------------------------

	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual bool IsMovingOnGround() const override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual bool CanAttemptJump() const override;
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;
	virtual void Crouch(bool bClientSimulation = false) override;
	virtual void UnCrouch(bool bClientSimulation = false) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	virtual void InitializeComponent() override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void SetPostLandedPhysics(const FHitResult& Hit) override;

public:
	// ============================ Tuning ======================================================

	// ---- Ground speeds ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float JogSpeed = 520.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SprintSpeed = 780.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float CrouchSpeed = 300.f;

	/** Sprint only applies when moving within this angle of the camera's forward. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Speeds", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "deg"))
	float SprintMaxAngle = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Speeds")
	float SprintAcceleration = 3000.f;

	// ---- Slide ----
	/** Minimum horizontal speed needed to start a slide from a sprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Slide", meta = (ForceUnits = "cm/s"))
	float SlideMinEnterSpeed = 560.f;

	/** Slide ends (becomes a crouch) below this speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Slide", meta = (ForceUnits = "cm/s"))
	float SlideMinSpeed = 340.f;

	/** Speed added along the movement direction when a slide starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Slide", meta = (ForceUnits = "cm/s"))
	float SlideEnterBoost = 460.f;

	/** Absolute cap on slide speed (protects against slope + slide-hop runaway). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Slide", meta = (ForceUnits = "cm/s"))
	float SlideMaxSpeed = 1500.f;

	/** Downhill pull while sliding. Sliding down slopes gains speed, uphill loses it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Slide")
	float SlideGravityForce = 4000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Slide")
	float SlideFriction = 0.7f;

	/** Sideways steering acceleration while sliding. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Slide")
	float SlideSteerAcceleration = 900.f;

	/** The enter boost is only granted once per this many seconds (stops boost-spam). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Slide", meta = (ForceUnits = "s"))
	float SlideBoostCooldown = 1.0f;

	/** Jumping out of a slide keeps momentum and adds this horizontal boost (slide-hop). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Slide", meta = (ForceUnits = "cm/s"))
	float SlideHopBoost = 160.f;

	// ---- Jump / double jump ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Jump", meta = (ForceUnits = "cm/s"))
	float DoubleJumpZVelocity = 620.f;

	/** 0 = keep current horizontal direction, 1 = snap fully to stick direction on double jump. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Jump", meta = (ClampMin = "0", ClampMax = "1"))
	float DoubleJumpRedirect = 0.75f;

	/** Seconds after running off a ledge during which the ground jump still works. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Jump", meta = (ForceUnits = "s"))
	float CoyoteTime = 0.15f;

	// ---- Wall-run ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|WallRun", meta = (ForceUnits = "cm/s"))
	float WallRunSpeed = 820.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|WallRun", meta = (ForceUnits = "cm/s"))
	float WallRunMinEnterSpeed = 450.f;

	/** Wall-run ends below this horizontal speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|WallRun", meta = (ForceUnits = "cm/s"))
	float WallRunMinSpeed = 300.f;

	/** Must be at least this high above the ground to start a wall-run. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|WallRun", meta = (ForceUnits = "cm"))
	float WallRunMinHeight = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|WallRun", meta = (ForceUnits = "s"))
	float WallRunMaxDuration = 1.75f;

	/** Gravity scale during a wall-run, ramped from Start to End over the run. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|WallRun")
	float WallRunGravityScaleStart = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|WallRun")
	float WallRunGravityScaleEnd = 0.6f;

	/** Upward speed kept when entering a wall-run is capped to this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|WallRun", meta = (ForceUnits = "cm/s"))
	float WallRunMaxEnterZVelocity = 250.f;

	/** How far from the capsule edge we look for a wall. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|WallRun", meta = (ForceUnits = "cm"))
	float WallRunTraceDistance = 45.f;

	/** Walls steeper than this many degrees from vertical are not runnable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|WallRun", meta = (ForceUnits = "deg"))
	float WallRunMaxWallAngle = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|WallRun", meta = (ForceUnits = "cm/s"))
	float WallJumpOffSpeed = 520.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|WallRun", meta = (ForceUnits = "cm/s"))
	float WallJumpUpSpeed = 600.f;

	/** The same wall (same normal) can't be re-run until landing or this much time has passed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|WallRun", meta = (ForceUnits = "s"))
	float WallRunSameWallCooldown = 1.0f;

	// ---- Mantle ----
	/** Ledges lower than this (above the feet) are stepped/jumped, not mantled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Mantle", meta = (ForceUnits = "cm"))
	float MantleMinHeight = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Mantle", meta = (ForceUnits = "cm"))
	float MantleMaxHeight = 230.f;

	/** How far ahead of the capsule a ledge can be detected. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Mantle", meta = (ForceUnits = "cm"))
	float MantleReach = 60.f;

	/** Mantle duration for a low ledge (MantleMinHeight) and a high ledge (MantleMaxHeight). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Mantle", meta = (ForceUnits = "s"))
	float MantleDurationLow = 0.28f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Mantle", meta = (ForceUnits = "s"))
	float MantleDurationHigh = 0.55f;

	/** Speed kept moving forward after a mantle completes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Mantle", meta = (ForceUnits = "cm/s"))
	float MantleExitSpeed = 350.f;

	/** While airborne, pushing forward into a ledge mantles automatically (no button needed). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Mantle")
	bool bAutoMantleInAir = true;

	// ---- Dash ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Dash", meta = (ForceUnits = "cm/s"))
	float DashSpeed = 1900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Dash", meta = (ForceUnits = "s"))
	float DashDuration = 0.16f;

	/** Horizontal speed is clamped to this when the dash ends. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Dash", meta = (ForceUnits = "cm/s"))
	float DashExitSpeed = 820.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Dash", meta = (ForceUnits = "s"))
	float DashCooldown = 1.1f;

	/** Dashes allowed per airtime (refilled on landing / wall-run). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Dash")
	int32 MaxAirDashes = 1;

	// ---- Air ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZS|Movement|Air")
	float ZSAirControl = 0.55f;

	float ExternalSpeedScale = 1.f;

private:
	// ---- Intent (replicated-ready via compressed flags) ----
	bool bWantsToSprint = false;
	bool bWantsToDash = false;

	// ---- Slide state ----
	bool bWantsToSlide = false;
	float SlideBoostCooldownRemaining = 0.f;

	// ---- Dash state ----
	float DashTimeRemaining = 0.f;
	float DashCooldownRemaining = 0.f;
	int32 AirDashesRemaining = 1;
	FVector DashDirection = FVector::ForwardVector;

	// ---- Jump state ----
	float TimeSinceLeftGround = 0.f;
	bool bLeftGroundByJumping = false;

	// ---- Wall-run state ----
	EZSWallSide WallRunSide = EZSWallSide::None;
	FVector WallRunNormal = FVector::ZeroVector;
	float WallRunElapsed = 0.f;
	FVector LastWallRunNormal = FVector::ZeroVector;
	float LastWallRunEndTime = -100.f;

	// ---- Mantle state ----
	FVector MantleStartLocation = FVector::ZeroVector;
	FVector MantleTargetLocation = FVector::ZeroVector;
	float MantleDuration = 0.f;
	float MantleElapsed = 0.f;
	FVector MantleForward = FVector::ForwardVector;

	bool bLastCrouchMaintainedBase = false;


	// ---- Helpers ----
	bool IsMovingForward(float MaxAngleDeg) const;
	float GetHorizontalSpeed() const { return Velocity.Size2D(); }
	FVector GetInputDirection2D() const;
	float GetWorldTime() const;

	// Slide
	bool CanSlide() const;
	void EnterSlide();
	void ExitSlide(bool bStandUp);
	bool FindSlideSurface(FHitResult& OutHit) const;
	void PhysSlide(float DeltaTime, int32 Iterations);

	// Wall-run
	bool TryStartWallRun();
	bool FindRunnableWall(EZSWallSide& OutSide, FHitResult& OutHit) const;
	bool IsWallRunnable(const FHitResult& Hit) const;
	void ExitWallRun();
	void PhysWallRun(float DeltaTime, int32 Iterations);

	// Mantle
	bool TryStartMantle();
	void PhysMantle(float DeltaTime, int32 Iterations);

	// Dash
	void TickDash(float DeltaSeconds);
	bool TryStartDash();

	void SetCustomMode(EZSCustomMovementMode Mode);
	float GetCapsuleRadius() const;
	float GetCapsuleHalfHeight() const;
};

/** Saved move carrying our extra intent bits, so the component works the same if networking is ever added. */
class FZSSavedMove : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	uint8 bSavedWantsToSprint : 1;
	uint8 bSavedWantsToDash : 1;

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
};

class FZSNetworkPredictionData_Client : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	explicit FZSNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement);
	virtual FSavedMovePtr AllocateNewMove() override;
};
