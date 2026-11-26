#include "Input/ZSInputSet.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

UInputAction* UZSInputSet::MakeAction(const TCHAR* Name, EInputActionValueType ValueType)
{
	UInputAction* Action = NewObject<UInputAction>(this, FName(Name), RF_Transient);
	Action->ValueType = ValueType;
	return Action;
}

void UZSInputSet::MapKey(UInputAction* Action, const FKey& Key)
{
	GameplayContext->MapKey(Action, Key);
}

void UZSInputSet::MapWASD(UInputAction* Action)
{
	// D = +X
	GameplayContext->MapKey(Action, EKeys::D);

	// A = -X
	{
		FEnhancedActionKeyMapping& Mapping = GameplayContext->MapKey(Action, EKeys::A);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(GameplayContext));
	}

	// W = +Y (swizzle X into Y)
	{
		FEnhancedActionKeyMapping& Mapping = GameplayContext->MapKey(Action, EKeys::W);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(GameplayContext);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	}

	// S = -Y
	{
		FEnhancedActionKeyMapping& Mapping = GameplayContext->MapKey(Action, EKeys::S);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(GameplayContext);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(GameplayContext));
	}
}

void UZSInputSet::Build()
{
	if (IsBuilt())
	{
		return;
	}

	GameplayContext = NewObject<UInputMappingContext>(this, TEXT("IMC_ZS_Gameplay"), RF_Transient);

	// ---- Actions ----------------------------------------------------------------------------
	Move                = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookMouse           = MakeAction(TEXT("IA_LookMouse"), EInputActionValueType::Axis2D);
	LookGamepad         = MakeAction(TEXT("IA_LookGamepad"), EInputActionValueType::Axis2D);
	Jump                = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	CrouchSlide         = MakeAction(TEXT("IA_CrouchSlide"), EInputActionValueType::Boolean);
	SprintGamepad       = MakeAction(TEXT("IA_SprintGamepad"), EInputActionValueType::Boolean);
	SprintKeyboard      = MakeAction(TEXT("IA_SprintKeyboard"), EInputActionValueType::Boolean);
	Dash                = MakeAction(TEXT("IA_Dash"), EInputActionValueType::Boolean);

	Fire                = MakeAction(TEXT("IA_Fire"), EInputActionValueType::Boolean);
	AimDownSights       = MakeAction(TEXT("IA_AimDownSights"), EInputActionValueType::Boolean);
	Reload              = MakeAction(TEXT("IA_Reload"), EInputActionValueType::Boolean);
	Interact            = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
	SwitchWeapon        = MakeAction(TEXT("IA_SwitchWeapon"), EInputActionValueType::Boolean);
	Melee               = MakeAction(TEXT("IA_Melee"), EInputActionValueType::Boolean);
	Lethal              = MakeAction(TEXT("IA_Lethal"), EInputActionValueType::Boolean);
	Tactical            = MakeAction(TEXT("IA_Tactical"), EInputActionValueType::Boolean);

	Pause               = MakeAction(TEXT("IA_Pause"), EInputActionValueType::Boolean);
	ToggleMovementDebug = MakeAction(TEXT("IA_ToggleMovementDebug"), EInputActionValueType::Boolean);

	// ---- Gamepad (primary) ------------------------------------------------------------------
	// Sticks are mapped raw; dead zones and response curves are applied in C++ from user settings.
	MapKey(Move,                EKeys::Gamepad_Left2D);
	MapKey(LookGamepad,         EKeys::Gamepad_Right2D);
	MapKey(Jump,                EKeys::Gamepad_FaceButton_Bottom);
	MapKey(CrouchSlide,         EKeys::Gamepad_FaceButton_Right);
	MapKey(SprintGamepad,       EKeys::Gamepad_LeftThumbstick);
	MapKey(Dash,                EKeys::Gamepad_LeftShoulder);
	MapKey(Fire,                EKeys::Gamepad_RightTrigger);
	MapKey(AimDownSights,       EKeys::Gamepad_LeftTrigger);
	MapKey(Reload,              EKeys::Gamepad_FaceButton_Left);
	MapKey(Interact,            EKeys::Gamepad_FaceButton_Left);
	MapKey(SwitchWeapon,        EKeys::Gamepad_FaceButton_Top);
	MapKey(Melee,               EKeys::Gamepad_RightThumbstick);
	MapKey(Lethal,              EKeys::Gamepad_RightShoulder);
	MapKey(Tactical,            EKeys::Gamepad_DPad_Left);
	MapKey(Pause,               EKeys::Gamepad_Special_Right);
	MapKey(ToggleMovementDebug, EKeys::Gamepad_DPad_Down);

	// ---- Keyboard + mouse -------------------------------------------------------------------
	MapWASD(Move);
	MapKey(LookMouse,           EKeys::Mouse2D);
	MapKey(Jump,                EKeys::SpaceBar);
	MapKey(CrouchSlide,         EKeys::LeftControl);
	MapKey(CrouchSlide,         EKeys::C);
	MapKey(SprintKeyboard,      EKeys::LeftShift);
	MapKey(Dash,                EKeys::LeftAlt);
	MapKey(Dash,                EKeys::ThumbMouseButton2);
	MapKey(Fire,                EKeys::LeftMouseButton);
	MapKey(AimDownSights,       EKeys::RightMouseButton);
	MapKey(Reload,              EKeys::R);
	MapKey(Interact,            EKeys::E);
	MapKey(SwitchWeapon,        EKeys::MouseScrollUp);
	MapKey(SwitchWeapon,        EKeys::MouseScrollDown);
	MapKey(Melee,               EKeys::V);
	MapKey(Melee,               EKeys::ThumbMouseButton);
	MapKey(Lethal,              EKeys::G);
	MapKey(Tactical,            EKeys::Q);
	MapKey(Pause,               EKeys::Escape);
	MapKey(ToggleMovementDebug, EKeys::F3);
}
