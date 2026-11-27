#include "Player/ZSPlayerCharacter.h"
#include "Player/ZSCameraComponent.h"
#include "Player/ZSPlayerController.h"
#include "Movement/ZSMovementComponent.h"
#include "Input/ZSInputSet.h"
#include "Core/ZSGameUserSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Player/ZSFirstPersonArmsComponent.h"
#include "Weapons/ZSWeaponComponent.h"
#include "Combat/ZSHealthComponent.h"
#include "Combat/ZSBloodSubsystem.h"
#include "Interaction/ZSInteractionComponent.h"
#include "Core/ZSGameMode_Sandbox.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/ZSCharacterCatalog.h"
#include "Profile/ZSProfileSubsystem.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Weapons/ZSWeaponBase.h"
#include "Zombieshooter.h"

AZSPlayerCharacter::AZSPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UZSMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);

	// Eye heights are measured from the capsule centre: standing eye ~160 cm, crouched ~86 cm above the floor.
	BaseEyeHeight = 70.f;
	CrouchedEyeHeight = 34.f;

	JumpMaxCount = 2;       // ground jump + double jump
	JumpMaxHoldTime = 0.f;  // every press is a discrete jump

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	FirstPersonCamera = CreateDefaultSubobject<UZSCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));

	FirstPersonArms = CreateDefaultSubobject<UZSFirstPersonArmsComponent>(TEXT("FirstPersonArms"));
	FirstPersonArms->SetupAttachment(FirstPersonCamera);

	ViewModelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ViewModelRoot"));
	ViewModelRoot->SetupAttachment(FirstPersonCamera);

	AnimatedArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimatedArms"));
	AnimatedArms->SetupAttachment(ViewModelRoot);
	AnimatedArms->SetOnlyOwnerSee(true);
	AnimatedArms->SetCastShadow(false);
	AnimatedArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnimatedArms->SetVisibility(false);
	AnimatedArms->bUseAttachParentBound = false;
	AnimatedArms->SetBoundsScale(4.f);

	WeaponComponent = CreateDefaultSubobject<UZSWeaponComponent>(TEXT("WeaponComponent"));

	HealthComponent = CreateDefaultSubobject<UZSHealthComponent>(TEXT("Health"));
	HealthComponent->MaxHealth = 100.f;
	HealthComponent->RegenDelay = 4.f;
	HealthComponent->RegenRate = 30.f;
	// Armor plates (pickups): 3 x 50, starts empty.
	HealthComponent->MaxArmor = 150.f;
	HealthComponent->StartingArmor = 0.f;
	HealthComponent->ArmorAbsorption = 0.7f;

	Interaction = CreateDefaultSubobject<UZSInteractionComponent>(TEXT("Interaction"));

	// Full body (outfit) is for shadows / mirrors / third person; the owner never sees it directly.
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->bCastHiddenShadow = true;
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	ShoulderBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("ShoulderBoom"));
	ShoulderBoom->SetupAttachment(GetCapsuleComponent());
	ShoulderBoom->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	ShoulderBoom->TargetArmLength = 220.f;
	ShoulderBoom->SocketOffset = FVector(0.f, 55.f, 10.f);
	ShoulderBoom->bUsePawnControlRotation = true;
	ShoulderBoom->bEnableCameraLag = true;
	ShoulderBoom->CameraLagSpeed = 18.f;
	ShoulderCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ShoulderCamera"));
	ShoulderCamera->SetupAttachment(ShoulderBoom);
	ShoulderCamera->SetAutoActivate(false);
}

UZSMovementComponent* AZSPlayerCharacter::GetZSMovement() const
{
	return Cast<UZSMovementComponent>(GetCharacterMovement());
}

void AZSPlayerCharacter::BeginPlay()
{
	// The arms set must be chosen before the arms component's own BeginPlay picks its default.
	if (const UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this))
	{
		const int32 ArmsIndex = ZSCharacterCatalog::ResolveArms(Profile->GetLook());
		if (USkeletalMesh* Arms = ZSCharacterCatalog::LoadArms(ArmsIndex))
		{
			FirstPersonArms->ArmsMesh = Arms;
		}
	}
	Super::BeginPlay();
	CurrentEyeHeight = BaseEyeHeight;
	// Same gloved arms mesh as the code-posed arms; used with animated weapons.
	if (USkeletalMesh* ArmsMesh = FirstPersonArms->ArmsMesh.LoadSynchronous())
	{
		AnimatedArms->SetSkeletalMeshAsset(ArmsMesh);
	}
	HealthComponent->OnDamaged.AddDynamic(this, &ThisClass::HandleDamaged);
	HealthComponent->OnDeath.AddDynamic(this, &ThisClass::HandleDeath);

	ApplyLook();
	if (UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this))
	{
		LookChangedHandle = Profile->OnLookChanged.AddUObject(this, &ThisClass::ApplyLook);
	}
}

void AZSPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this))
	{
		Profile->OnLookChanged.Remove(LookChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AZSPlayerCharacter::ApplyLook()
{
	const UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this);
	if (!Profile)
	{
		return;
	}
	const FZSCharacterLook& Look = Profile->GetLook();

	// Body: shadow in first person, visible in third person.
	if (USkeletalMesh* Body = ZSCharacterCatalog::LoadBody(Look.Body))
	{
		GetMesh()->SetSkeletalMeshAsset(Body);
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		BodyIdle = ZSCharacterCatalog::LoadIdle(Look.Body);
		BodyRun = ZSCharacterCatalog::LoadRun(Look.Body);
		bBodyRunning = false;
		if (BodyIdle) { GetMesh()->PlayAnimation(BodyIdle, true); }
	}

	// Arms set + tint (both the code-posed and the animated-rig arms).
	const int32 ArmsIndex = ZSCharacterCatalog::ResolveArms(Look);
	const int32 Tint = ZSCharacterCatalog::ResolveTint(Look);
	if (USkeletalMesh* Arms = ZSCharacterCatalog::LoadArms(ArmsIndex))
	{
		if (FirstPersonArms->GetSkinnedAsset() != Arms)
		{
			FirstPersonArms->ArmsMesh = Arms;
			FirstPersonArms->SetArmsMesh(Arms);
		}
		if (AnimatedArms->GetSkinnedAsset() != Arms)
		{
			AnimatedArms->SetSkeletalMeshAsset(Arms);
		}
	}
	ZSCharacterCatalog::ApplyTint(FirstPersonArms, Tint);
	ZSCharacterCatalog::ApplyTint(AnimatedArms, Tint);
	// Re-show the right arms for the held weapon (SetArmsMesh changes visibility).
	if (WeaponComponent && WeaponComponent->GetCurrentWeapon())
	{
		WeaponComponent->SetupArmsFor(WeaponComponent->GetCurrentWeapon());
	}
	else
	{
		FirstPersonArms->SetVisibility(false);
	}
}

void AZSPlayerCharacter::UpdateBodyAnim()
{
	const bool bRun = GetVelocity().Size2D() > 120.f;
	if (bRun != bBodyRunning && GetMesh()->GetSkinnedAsset())
	{
		bBodyRunning = bRun;
		if (UAnimSequence* Anim = bRun ? BodyRun.Get() : BodyIdle.Get())
		{
			GetMesh()->PlayAnimation(Anim, true);
		}
	}
}

void AZSPlayerCharacter::SetThirdPerson(bool bEnable)
{
	bThirdPerson = bEnable;
	ShoulderCamera->SetActive(bEnable);
	FirstPersonCamera->SetActive(!bEnable);
	GetMesh()->SetOwnerNoSee(!bEnable);
	ViewModelRoot->SetVisibility(!bEnable, true);
	FirstPersonArms->SetHiddenInGame(bEnable);
	AnimatedArms->SetHiddenInGame(bEnable);
}

// =============================================================================================
// Tick
// =============================================================================================

void AZSPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBodyAnim();

	UZSMovementComponent* Move = GetZSMovement();

	// Toggle-sprint ends when the player stops moving, or when movement dropped the intent (e.g. crouched).
	if (bSprintToggled && Move)
	{
		TimeWithoutMoveInput = LastMoveInput.Size() < 0.1f ? TimeWithoutMoveInput + DeltaSeconds : 0.f;
		if (TimeWithoutMoveInput > 0.15f || !Move->GetSprintInput())
		{
			bSprintToggled = false;
			Move->SetSprintInput(false);
		}
	}

	UpdateEyeHeight(DeltaSeconds);
	FirstPersonCamera->UpdateFeel(DeltaSeconds, Move);

	// Dust off the floor while sliding.
	if (Move && Move->IsSliding())
	{
		SlideDustTimer -= DeltaSeconds;
		if (SlideDustTimer <= 0.f)
		{
			SlideDustTimer = 0.06f;
			if (UZSBloodSubsystem* FX = GetWorld()->GetSubsystem<UZSBloodSubsystem>())
			{
				const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.f);
				FX->SpawnDust(Feet + GetVelocity().GetSafeNormal2D() * 60.f, GetVelocity() * 0.25f, FMath::Clamp(GetVelocity().Size2D() / 900.f, 0.5f, 1.2f));
			}
		}
	}

	if (const AZSPlayerController* PC = Cast<AZSPlayerController>(GetController()))
	{
		if (PC->IsMovementDebugVisible())
		{
			DrawMovementDebug();
		}
	}
}

void AZSPlayerCharacter::UpdateEyeHeight(float DeltaSeconds)
{
	CurrentEyeHeight = FMath::FInterpTo(CurrentEyeHeight, BaseEyeHeight, DeltaSeconds, EyeHeightInterpSpeed);
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, CurrentEyeHeight));
}

void AZSPlayerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// The capsule centre just dropped; compensate so the camera glides down instead of snapping.
	if (const UZSMovementComponent* Move = GetZSMovement(); Move && Move->DidLastCrouchMaintainBase())
	{
		CurrentEyeHeight += ScaledHalfHeightAdjust;
	}
}

void AZSPlayerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	if (const UZSMovementComponent* Move = GetZSMovement(); Move && Move->DidLastCrouchMaintainBase())
	{
		CurrentEyeHeight -= ScaledHalfHeightAdjust;
	}
}

void AZSPlayerCharacter::Landed(const FHitResult& Hit)
{
	const float FallSpeed = -GetCharacterMovement()->Velocity.Z;
	Super::Landed(Hit);
	FirstPersonCamera->AddLandingImpact(FallSpeed);
}

// =============================================================================================
// Jump rules
// =============================================================================================

bool AZSPlayerCharacter::CanJumpInternal_Implementation() const
{
	// Engine default forbids jumping while crouched; we allow it (and slide-hops) - the movement
	// component stands the player up as part of the jump.
	return JumpIsAllowedInternal();
}

bool AZSPlayerCharacter::CanJumpWhileFalling() const
{
	// Coyote time: a ground jump is still allowed briefly after running off a ledge.
	const UZSMovementComponent* Move = GetZSMovement();
	return Move && Move->CanCoyoteJump();
}

// =============================================================================================
// Input
// =============================================================================================

void AZSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogZS, Error, TEXT("%s: input component is not an EnhancedInputComponent. Check DefaultInput.ini."), *GetName());
		return;
	}

	AZSPlayerController* PC = Cast<AZSPlayerController>(GetController());
	if (!PC)
	{
		UE_LOG(LogZS, Error, TEXT("%s: not possessed by an AZSPlayerController - input not bound."), *GetName());
		return;
	}

	const UZSInputSet* Set = PC->GetInputSet();

	EIC->BindAction(Set->Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	EIC->BindAction(Set->Move, ETriggerEvent::Completed, this, &ThisClass::Input_MoveCompleted);
	EIC->BindAction(Set->LookMouse, ETriggerEvent::Triggered, this, &ThisClass::Input_LookMouse);
	EIC->BindAction(Set->LookGamepad, ETriggerEvent::Triggered, this, &ThisClass::Input_LookGamepad);
	EIC->BindAction(Set->LookGamepad, ETriggerEvent::Completed, this, &ThisClass::Input_LookGamepadCompleted);

	EIC->BindAction(Set->Jump, ETriggerEvent::Started, this, &ThisClass::Input_JumpStarted);
	EIC->BindAction(Set->Jump, ETriggerEvent::Completed, this, &ThisClass::Input_JumpCompleted);
	EIC->BindAction(Set->CrouchSlide, ETriggerEvent::Started, this, &ThisClass::Input_CrouchStarted);
	EIC->BindAction(Set->CrouchSlide, ETriggerEvent::Completed, this, &ThisClass::Input_CrouchCompleted);
	EIC->BindAction(Set->SprintGamepad, ETriggerEvent::Started, this, &ThisClass::Input_SprintGamepadStarted);
	EIC->BindAction(Set->SprintGamepad, ETriggerEvent::Completed, this, &ThisClass::Input_SprintGamepadCompleted);
	EIC->BindAction(Set->SprintKeyboard, ETriggerEvent::Started, this, &ThisClass::Input_SprintKeyboardStarted);
	EIC->BindAction(Set->SprintKeyboard, ETriggerEvent::Completed, this, &ThisClass::Input_SprintKeyboardCompleted);
	EIC->BindAction(Set->Dash, ETriggerEvent::Started, this, &ThisClass::Input_Dash);
	EIC->BindAction(Set->ToggleMovementDebug, ETriggerEvent::Started, this, &ThisClass::Input_ToggleMovementDebug);

	EIC->BindAction(Set->Fire, ETriggerEvent::Started, this, &ThisClass::Input_FireStarted);
	EIC->BindAction(Set->Fire, ETriggerEvent::Completed, this, &ThisClass::Input_FireCompleted);
	EIC->BindAction(Set->AimDownSights, ETriggerEvent::Started, this, &ThisClass::Input_ADSStarted);
	EIC->BindAction(Set->AimDownSights, ETriggerEvent::Completed, this, &ThisClass::Input_ADSCompleted);
	EIC->BindAction(Set->Reload, ETriggerEvent::Started, this, &ThisClass::Input_Reload);
	EIC->BindAction(Set->SwitchWeapon, ETriggerEvent::Started, this, &ThisClass::Input_SwitchWeapon);
	EIC->BindAction(Set->FireMode, ETriggerEvent::Started, this, &ThisClass::Input_FireMode);
	EIC->BindAction(Set->Interact, ETriggerEvent::Started, this, &ThisClass::Input_Interact);
	EIC->BindAction(Set->Melee, ETriggerEvent::Started, this, &ThisClass::Input_Melee);
}

FVector2D AZSPlayerCharacter::ApplyRadialDeadZone(const FVector2D& Raw, float Inner, float Outer)
{
	const float Magnitude = Raw.Size();
	if (Magnitude <= Inner || Magnitude <= KINDA_SMALL_NUMBER)
	{
		return FVector2D::ZeroVector;
	}
	const float Scaled = FMath::Clamp((Magnitude - Inner) / FMath::Max(Outer - Inner, 0.01f), 0.f, 1.f);
	return Raw / Magnitude * Scaled;
}

void AZSPlayerCharacter::Input_Move(const FInputActionValue& Value)
{
	const UZSGameUserSettings* Settings = UZSGameUserSettings::Get();

	// Radial dead zone for the stick; keyboard values (0 / 1 / 1.41 diagonal) come out normalised.
	const FVector2D Input = ApplyRadialDeadZone(Value.Get<FVector2D>(), Settings->GamepadMoveDeadZone, Settings->GamepadOuterDeadZone);
	LastMoveInput = Input;
	if (Input.IsNearlyZero() || !Controller)
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(YawRotation);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Input.X);
}

void AZSPlayerCharacter::Input_MoveCompleted(const FInputActionValue& Value)
{
	LastMoveInput = FVector2D::ZeroVector;
}

void AZSPlayerCharacter::Input_LookMouse(const FInputActionValue& Value)
{
	const UZSGameUserSettings* Settings = UZSGameUserSettings::Get();
	const FVector2D Delta = Value.Get<FVector2D>();
	const float Scale = MouseDegreesPerCount * Settings->MouseSensitivity * (WeaponComponent ? WeaponComponent->GetLookInputScale(false) : 1.f);
	const float InvertY = Settings->bInvertMouseY ? -1.f : 1.f;

	AddControllerYawInput(Delta.X * Scale);
	AddControllerPitchInput(Delta.Y * Scale * InvertY);
}

void AZSPlayerCharacter::Input_LookGamepad(const FInputActionValue& Value)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float DeltaSeconds = World->GetDeltaSeconds();
	const UZSGameUserSettings* Settings = UZSGameUserSettings::Get();

	const FVector2D Stick = ApplyRadialDeadZone(Value.Get<FVector2D>(), Settings->GamepadLookDeadZone, Settings->GamepadOuterDeadZone);
	const float Magnitude = Stick.Size();
	if (Magnitude <= 0.f)
	{
		GamepadFullDeflectionTime = 0.f;
		return;
	}

	// Response curve on the magnitude keeps the direction exact while giving fine control near centre.
	const FVector2D Direction = Stick / Magnitude;
	const float Curved = FMath::Pow(Magnitude, Settings->GamepadResponseExponent);
	const float Sensitivity = Settings->GamepadLookSensitivity * (WeaponComponent ? WeaponComponent->GetLookInputScale(true) : 1.f);
	// The owner's controller reports stick-up as -Y, so the non-inverted default flips it (push up = look up).
	const float InvertY = Settings->bInvertGamepadY ? 1.f : -1.f;

	float YawRate = Direction.X * Curved * GamepadYawRate * Sensitivity;
	const float PitchRate = Direction.Y * Curved * GamepadPitchRate * Sensitivity * InvertY;

	// Look acceleration: holding the stick fully sideways ramps up turn speed for fast 180s.
	if (Settings->bGamepadLookAcceleration && Magnitude >= 0.97f && FMath::Abs(Direction.X) > 0.8f)
	{
		GamepadFullDeflectionTime += DeltaSeconds;
		const float Ramp = FMath::Clamp((GamepadFullDeflectionTime - GamepadAccelDelay) / FMath::Max(GamepadAccelRampTime, 0.01f), 0.f, 1.f);
		YawRate += FMath::Sign(Direction.X) * Ramp * GamepadAccelExtraYawRate * Sensitivity;
	}
	else
	{
		GamepadFullDeflectionTime = 0.f;
	}

	AddControllerYawInput(YawRate * DeltaSeconds);
	AddControllerPitchInput(PitchRate * DeltaSeconds);
}

void AZSPlayerCharacter::Input_LookGamepadCompleted(const FInputActionValue& Value)
{
	GamepadFullDeflectionTime = 0.f;
}

void AZSPlayerCharacter::Input_JumpStarted(const FInputActionValue& Value)
{
	Jump();
}

void AZSPlayerCharacter::Input_JumpCompleted(const FInputActionValue& Value)
{
	StopJumping();
}

void AZSPlayerCharacter::Input_CrouchStarted(const FInputActionValue& Value)
{
	if (UZSMovementComponent* Move = GetZSMovement())
	{
		Move->HandleCrouchPressed(UZSGameUserSettings::Get()->bToggleCrouch);
	}
}

void AZSPlayerCharacter::Input_CrouchCompleted(const FInputActionValue& Value)
{
	if (UZSMovementComponent* Move = GetZSMovement())
	{
		Move->HandleCrouchReleased(UZSGameUserSettings::Get()->bToggleCrouch);
	}
}

void AZSPlayerCharacter::StartSprint(bool bToggleMode)
{
	UZSMovementComponent* Move = GetZSMovement();
	if (!Move)
	{
		return;
	}

	// Sprinting from a crouch stands you up.
	if (bIsCrouched && !Move->IsSliding())
	{
		UnCrouch();
	}

	if (bToggleMode)
	{
		bSprintToggled = !bSprintToggled;
		TimeWithoutMoveInput = 0.f;
		Move->SetSprintInput(bSprintToggled);
	}
	else
	{
		Move->SetSprintInput(true);
	}
}

void AZSPlayerCharacter::StopSprint(bool bToggleMode)
{
	if (!bToggleMode)
	{
		if (UZSMovementComponent* Move = GetZSMovement())
		{
			Move->SetSprintInput(false);
		}
	}
}

void AZSPlayerCharacter::Input_SprintGamepadStarted(const FInputActionValue& Value)
{
	StartSprint(UZSGameUserSettings::Get()->bToggleSprintGamepad);
}

void AZSPlayerCharacter::Input_SprintGamepadCompleted(const FInputActionValue& Value)
{
	StopSprint(UZSGameUserSettings::Get()->bToggleSprintGamepad);
}

void AZSPlayerCharacter::Input_SprintKeyboardStarted(const FInputActionValue& Value)
{
	StartSprint(UZSGameUserSettings::Get()->bToggleSprintKeyboard);
}

void AZSPlayerCharacter::Input_SprintKeyboardCompleted(const FInputActionValue& Value)
{
	StopSprint(UZSGameUserSettings::Get()->bToggleSprintKeyboard);
}

void AZSPlayerCharacter::Input_Dash(const FInputActionValue& Value)
{
	if (UZSMovementComponent* Move = GetZSMovement())
	{
		Move->RequestDash();
	}
}

void AZSPlayerCharacter::Input_ToggleMovementDebug(const FInputActionValue& Value)
{
	if (AZSPlayerController* PC = Cast<AZSPlayerController>(GetController()))
	{
		PC->ToggleMovementDebug();
	}
}

void AZSPlayerCharacter::CancelSprint()
{
	bSprintToggled = false;
	if (UZSMovementComponent* Move = GetZSMovement())
	{
		Move->SetSprintInput(false);
	}
}

void AZSPlayerCharacter::Input_FireStarted(const FInputActionValue& Value) { if (WeaponComponent) { WeaponComponent->SetTriggerHeld(true); } }
void AZSPlayerCharacter::Input_FireCompleted(const FInputActionValue& Value) { if (WeaponComponent) { WeaponComponent->SetTriggerHeld(false); } }
void AZSPlayerCharacter::Input_ADSStarted(const FInputActionValue& Value) { if (WeaponComponent) { WeaponComponent->SetADSHeld(true); } }
void AZSPlayerCharacter::Input_ADSCompleted(const FInputActionValue& Value) { if (WeaponComponent) { WeaponComponent->SetADSHeld(false); } }
void AZSPlayerCharacter::Input_Reload(const FInputActionValue& Value)
{
	// X is both Reload and Interact on the controller: looking at an interactable means interact, not reload.
	if (Interaction && Interaction->GetFocused())
	{
		return;
	}
	if (WeaponComponent) { WeaponComponent->RequestReload(); }
}

void AZSPlayerCharacter::Input_Interact(const FInputActionValue& Value)
{
	if (Interaction)
	{
		Interaction->TryInteract();
	}
}
void AZSPlayerCharacter::Input_SwitchWeapon(const FInputActionValue& Value) { if (WeaponComponent) { WeaponComponent->RequestSwitchWeapon(); } }
void AZSPlayerCharacter::Input_FireMode(const FInputActionValue& Value) { if (WeaponComponent) { WeaponComponent->RequestCycleFireMode(); } }
void AZSPlayerCharacter::Input_Melee(const FInputActionValue& Value) { if (WeaponComponent) { WeaponComponent->RequestMelee(); } }

// =============================================================================================
// Debug
// =============================================================================================

void AZSPlayerCharacter::DrawMovementDebug() const
{
	if (!GEngine)
	{
		return;
	}
	const UZSMovementComponent* Move = GetZSMovement();
	if (!Move)
	{
		return;
	}

	const UZSGameUserSettings* Settings = UZSGameUserSettings::Get();
	GEngine->AddOnScreenDebugMessage(7001, 0.f, FColor::Green, FString::Printf(TEXT("[ZS] %s"), *Move->GetDebugStateString()));
	GEngine->AddOnScreenDebugMessage(7002, 0.f, FColor::Silver, FString::Printf(TEXT("[ZS] FOV %.0f (+%.1f kick)  eye %.0f  move (%.2f, %.2f)  sprint-toggle %s"),
		Settings->FieldOfView, FirstPersonCamera->FieldOfView - Settings->FieldOfView, CurrentEyeHeight,
		LastMoveInput.X, LastMoveInput.Y, bSprintToggled ? TEXT("on") : TEXT("off")));
	GEngine->AddOnScreenDebugMessage(7003, 0.f, FColor::Silver,
		TEXT("[ZS] Pad: L3 sprint | A jump x2 / mantle | B crouch-slide | LB dash | D-pad Down hide.   KB: Shift / Space / Ctrl,C / Alt / F3"));
}

// =============================================================================================
// Health
// =============================================================================================

void AZSPlayerCharacter::HandleDamaged(UZSHealthComponent* InHealth, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	LastDamageTime = GetWorld()->GetTimeSeconds();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->PlayDynamicForceFeedback(0.7f, 0.2f, true, true, true, true);
	}
}

void AZSPlayerCharacter::HandleDeath(UZSHealthComponent* InHealth, AController* Killer, AActor* DamageCauser)
{
	const bool bHandledByMode = [this]()
	{
		AZSGameMode_Sandbox* GM = GetWorld()->GetAuthGameMode<AZSGameMode_Sandbox>();
		return GM && GM->HandlePlayerDeath(this);
	}();
	// Sandbox behaviour: freeze, then respawn. Milestone 5's survival mode turns this into game over.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
	CancelSprint();
	if (WeaponComponent)
	{
		WeaponComponent->SetTriggerHeld(false);
		WeaponComponent->SetADSHeld(false);
	}
	if (bHandledByMode)
	{
		return;
	}
	FTimerHandle Respawn;
	GetWorldTimerManager().SetTimer(Respawn, FTimerDelegate::CreateUObject(this, &ThisClass::SandboxRespawn), SandboxRespawnDelay, false);
}

void AZSPlayerCharacter::SandboxRespawn()
{
	if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
	{
		if (AActor* Start = GM->FindPlayerStart(GetController()))
		{
			TeleportTo(Start->GetActorLocation(), Start->GetActorRotation());
			if (Controller)
			{
				Controller->SetControlRotation(Start->GetActorRotation());
			}
		}
	}
	HealthComponent->Revive();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		EnableInput(PC);
	}
	if (WeaponComponent)
	{
		WeaponComponent->RefillAllAmmo();
	}
}
