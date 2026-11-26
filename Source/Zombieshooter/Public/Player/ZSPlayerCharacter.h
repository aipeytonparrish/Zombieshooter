#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZSPlayerCharacter.generated.h"

class UZSCameraComponent;
class UZSMovementComponent;
class UZSFirstPersonArmsComponent;
class UZSWeaponComponent;
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

	/** Scene component the held weapon is attached to (moved by the weapon component every frame). */
	USceneComponent* GetViewModelRoot() const { return ViewModelRoot; }

	UFUNCTION(BlueprintPure, Category = "ZS|Player")
	UZSWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	/** Ends sprinting (toggle or held). Used when firing or aiming. */
	void CancelSprint();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

protected:
	virtual void BeginPlay() override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual bool CanJumpWhileFalling() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZS|Player")
	TObjectPtr<UZSCameraComponent> FirstPersonCamera;

	/** First-person arms (owner-only), posed by IK onto the held weapon. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZS|Player")
	TObjectPtr<UZSFirstPersonArmsComponent> FirstPersonArms;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZS|Player")
	TObjectPtr<USceneComponent> ViewModelRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZS|Player")
	TObjectPtr<UZSWeaponComponent> WeaponComponent;

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

	void StartSprint(bool bToggleMode);
	void StopSprint(bool bToggleMode);

	/** Radial dead zone + rescale. Returns 0 inside the inner dead zone. */
	static FVector2D ApplyRadialDeadZone(const FVector2D& Raw, float Inner, float Outer);

	void UpdateEyeHeight(float DeltaSeconds);
	void DrawMovementDebug() const;

	float GamepadFullDeflectionTime = 0.f;
	float CurrentEyeHeight = 0.f;
	FVector2D LastMoveInput = FVector2D::ZeroVector;
	float TimeWithoutMoveInput = 0.f;
	bool bSprintToggled = false;
};
