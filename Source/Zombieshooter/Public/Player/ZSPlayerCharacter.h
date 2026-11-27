#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZSPlayerCharacter.generated.h"

class UZSCameraComponent;
class UZSMovementComponent;
class UZSFirstPersonArmsComponent;
class UZSWeaponComponent;
class UZSHealthComponent;
class UZSInteractionComponent;
class USpringArmComponent;
class UCameraComponent;
class UAnimSequence;
struct FInputActionValue;

/**
 * The player. Milestone 2: first-person camera, full movement kit, controller-first look handling.
 * Weapons (M3), health/armor (M5) and outfit-matched arms (M9) attach to this class later.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AZSPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "ZS|Player")
	UZSMovementComponent* GetZSMovement() const;

	UFUNCTION(BlueprintPure, Category = "ZS|Player")
	UZSCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintPure, Category = "ZS|Player")
	UZSFirstPersonArmsComponent* GetFirstPersonArms() const { return FirstPersonArms; }

	/** Gloved arms that copy the pose of an animated weapon rig (leader pose). */
	USkeletalMeshComponent* GetAnimatedArms() const { return AnimatedArms; }

	/** Scene component the held weapon is attached to (moved by the weapon component every frame). */
	USceneComponent* GetViewModelRoot() const { return ViewModelRoot; }

	UFUNCTION(BlueprintPure, Category = "ZS|Player")
	UZSWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	UFUNCTION(BlueprintPure, Category = "ZS|Player")
	UZSHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UZSInteractionComponent* GetInteraction() const { return Interaction; }

	/** World time the player last took damage (HUD damage flash). */
	float GetLastDamageTime() const { return LastDamageTime; }

	/** Ends sprinting (toggle or held). Used when firing or aiming. */
	void CancelSprint();

	/** Applies the profile's character look: survivor body (shadow / third person), arms set and tint. */
	void ApplyLook();

	/** Over-the-shoulder camera experiment (console: ZSThirdPerson). */
	void SetThirdPerson(bool bEnable);
	bool IsThirdPerson() const { return bThirdPerson; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual bool CanJumpWhileFalling() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZS|Player")
	TObjectPtr<UZSCameraComponent> FirstPersonCamera;

	/** First-person arms (owner-only), posed by IK onto the held weapon. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZS|Player")
	TObjectPtr<UZSFirstPersonArmsComponent> FirstPersonArms;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZS|Player")
	TObjectPtr<USkeletalMeshComponent> AnimatedArms;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZS|Player")
	TObjectPtr<USceneComponent> ViewModelRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZS|Player")
	TObjectPtr<UZSWeaponComponent> WeaponComponent;

	/** Health + regen. Armor arrives in Milestone 5. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZS|Player")
	TObjectPtr<UZSHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZS|Player")
	TObjectPtr<UZSInteractionComponent> Interaction;

	/** Third-person experiment camera (inactive by default). */
	UPROPERTY(VisibleAnywhere, Category = "ZS|Player")
	TObjectPtr<USpringArmComponent> ShoulderBoom;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Player")
	TObjectPtr<UCameraComponent> ShoulderCamera;

	/** Sandbox only: seconds before respawning after death (the round system replaces this with game over). */
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Player")
	float SandboxRespawnDelay = 3.f;

	// ---- Look tuning (controller) ----

	/** Right-stick yaw speed at full deflection, before sensitivity. */
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Look", meta = (ForceUnits = "deg/s"))
	float GamepadYawRate = 240.f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Look", meta = (ForceUnits = "deg/s"))
	float GamepadPitchRate = 170.f;

	/** Extra yaw speed added after holding full deflection (look acceleration). */
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Look", meta = (ForceUnits = "deg/s"))
	float GamepadAccelExtraYawRate = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Look", meta = (ForceUnits = "s"))
	float GamepadAccelDelay = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "ZS|Look", meta = (ForceUnits = "s"))
	float GamepadAccelRampTime = 0.35f;

	/** Degrees of rotation per mouse count at sensitivity 1. */
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Look")
	float MouseDegreesPerCount = 0.05f;

	/** Camera eye height interpolation speed when crouching / sliding. */
	UPROPERTY(EditDefaultsOnly, Category = "ZS|Camera")
	float EyeHeightInterpSpeed = 14.f;

private:
	void UpdateBodyAnim();

	bool bThirdPerson = false;
	bool bBodyRunning = false;
	FDelegateHandle LookChangedHandle;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> BodyIdle;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> BodyRun;

	// ---- Input handlers ----
	void Input_Move(const FInputActionValue& Value);
	void Input_MoveCompleted(const FInputActionValue& Value);
	void Input_LookMouse(const FInputActionValue& Value);
	void Input_LookGamepad(const FInputActionValue& Value);
	void Input_LookGamepadCompleted(const FInputActionValue& Value);
	void Input_JumpStarted(const FInputActionValue& Value);
	void Input_JumpCompleted(const FInputActionValue& Value);
	void Input_CrouchStarted(const FInputActionValue& Value);
	void Input_CrouchCompleted(const FInputActionValue& Value);
	void Input_SprintGamepadStarted(const FInputActionValue& Value);
	void Input_SprintGamepadCompleted(const FInputActionValue& Value);
	void Input_SprintKeyboardStarted(const FInputActionValue& Value);
	void Input_SprintKeyboardCompleted(const FInputActionValue& Value);
	void Input_Dash(const FInputActionValue& Value);
	void Input_ToggleMovementDebug(const FInputActionValue& Value);
	void Input_FireStarted(const FInputActionValue& Value);
	void Input_FireCompleted(const FInputActionValue& Value);
	void Input_ADSStarted(const FInputActionValue& Value);
	void Input_ADSCompleted(const FInputActionValue& Value);
	void Input_Reload(const FInputActionValue& Value);
	void Input_SwitchWeapon(const FInputActionValue& Value);
	void Input_FireMode(const FInputActionValue& Value);
	void Input_Interact(const FInputActionValue& Value);
	void Input_Melee(const FInputActionValue& Value);
	float SlideDustTimer = 0.f;
	float LowHealthSaturation = 1.f;

	void StartSprint(bool bToggleMode);
	void StopSprint(bool bToggleMode);

	/** Radial dead zone + rescale. Returns 0 inside the inner dead zone. */
	static FVector2D ApplyRadialDeadZone(const FVector2D& Raw, float Inner, float Outer);

	void UpdateEyeHeight(float DeltaSeconds);
	void DrawMovementDebug() const;

	UFUNCTION()
	void HandleDamaged(UZSHealthComponent* InHealth, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void HandleDeath(UZSHealthComponent* InHealth, AController* Killer, AActor* DamageCauser);

	void SandboxRespawn();

	float LastDamageTime = -10.f;

	float GamepadFullDeflectionTime = 0.f;
	float CurrentEyeHeight = 0.f;
	FVector2D LastMoveInput = FVector2D::ZeroVector;
	float TimeWithoutMoveInput = 0.f;
	bool bSprintToggled = false;
};
