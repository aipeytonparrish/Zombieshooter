#include "Weapons/ZSWeapon_TacticalAR.h"
#include "Weapons/ZSAssetPath.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Engine/SkeletalMesh.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

using ZSAssets::Soft;

namespace
{
	const FString TFA = TEXT("/Game/InfimaGames/TacticalFPSAnimations");
	const FString FP = TFA + TEXT("/Weapons/AssaultRifle/Animations/Character/FP/");
	const FString WEP = TFA + TEXT("/Weapons/AssaultRifle/Animations/Weapon/");
	const FString AUD = TFA + TEXT("/Weapons/AssaultRifle/Audio/");
}

AZSWeapon_TacticalAR::AZSWeapon_TacticalAR()
{
	FZSWeaponStats& S = Stats;
	S.DisplayName = NSLOCTEXT("ZS", "TacticalAR", "M4 Tactical");
	S.WeaponClass = EZSWeaponClass::AssaultRifle;

	S.Damage = 32.f;
	S.HeadshotMultiplier = 2.f;
	S.FalloffStart = 2500.f;
	S.FalloffEnd = 6000.f;
	S.MinDamageMultiplier = 0.65f;
	S.PenetrationDepth = 22.f;
	S.MaxPenetrations = 2;

	S.FireRateRPM = 720.f;
	S.FireModes = { EZSFireMode::Auto, EZSFireMode::Burst, EZSFireMode::Semi };
	S.MagSize = 30;
	S.MaxReserve = 210;
	S.TacticalReloadTime = 2.6f;   // pack clip is 3.67 s; played faster
	S.EmptyReloadTime = 2.95f;
	S.ReloadCommitFraction = 0.62f;

	S.RecoilPattern.Reset();
	for (int32 i = 0; i < 30; ++i)
	{
		S.RecoilPattern.Add(FVector2D(i < 10 ? 0.05f : (i < 20 ? 0.14f : -0.14f), i < 4 ? 0.45f : 0.27f));
	}
	S.RecoilRandomYaw = 0.1f;
	S.RecoilADSScale = 0.7f;

	S.HipSpread = 2.0f;
	S.ADSSpread = 0.08f;
	S.SpreadPerShot = 0.25f;
	S.ADSTime = 0.24f;
	S.ADSFOVMultiplier = 0.82f;
	S.EquipTime = 1.0f;
	S.UnequipTime = 0.35f;
	S.VisualKickBack = 0.6f;    // the clips carry their own recoil motion
	S.VisualKickPitch = 0.4f;
	S.SwayScale = 0.7f;

	// ---- Animated rig (InfimaGames TFA) ----
	FZSWeaponAnimSet& A = AnimSet;
	A.DriverMesh = Soft<USkeletalMesh>(TFA + TEXT("/Common/Characters/Mannequins/Meshes/SKM_FP_Manny_Simple"));
	A.WeaponMesh = Soft<USkeletalMesh>(TFA + TEXT("/Weapons/AssaultRifle/Meshes/SK_TFA_AR_Default_Combined"));
	A.WeaponAttachBone = TEXT("ik_hand_gun");
	A.CameraSocket = TEXT("SOCKET_CameraFP");
	A.CameraFromSocket = FTransform(FRotator(0.f, 90.f, -90.f));   // as in the pack's BP_TFA_BaseCharacter

	A.Idle = Soft<UAnimSequence>(FP + TEXT("Locomotion/A_TFA_FP_AR_Idle_Loop_Standing"));
	A.Walk = Soft<UAnimSequence>(FP + TEXT("Locomotion/A_TFA_FP_AR_Walk_F_Loop_Standing"));
	A.Sprint = Soft<UAnimSequence>(FP + TEXT("Locomotion/A_TFA_FP_AR_Sprint_F_Loop"));
	A.AimIdle = Soft<UAnimSequence>(FP + TEXT("Poses/A_TFA_FP_AR_Aim_Pose"));   // the aimed loops are motion layers over this pose
	A.AimWalk = A.AimIdle;
	A.Fire = Soft<UAnimSequence>(FP + TEXT("Combat/A_TFA_FP_AR_Fire_Semi"));
	A.FireAimed = Soft<UAnimSequence>(FP + TEXT("Combat/A_TFA_FP_AR_Fire_Aimed"));
	A.Reload = Soft<UAnimSequence>(FP + TEXT("Combat/A_TFA_FP_AR_Reload"));
	A.ReloadEmpty = Soft<UAnimSequence>(FP + TEXT("Combat/A_TFA_FP_AR_Reload_Empty"));
	A.Equip = Soft<UAnimSequence>(FP + TEXT("Transitions/A_TFA_FP_AR_Equip_Quick"));
	A.Holster = Soft<UAnimSequence>(FP + TEXT("Transitions/A_TFA_FP_AR_Holster"));

	A.WeaponIdle = Soft<UAnimSequence>(WEP + TEXT("Common/A_TFA_WEP_AR_Reference"));
	A.WeaponFire = Soft<UAnimSequence>(WEP + TEXT("FP/A_TFA_FP_WEP_AR_Fire"));
	A.WeaponReload = Soft<UAnimSequence>(WEP + TEXT("FP/A_TFA_FP_WEP_AR_Reload"));
	A.WeaponReloadEmpty = Soft<UAnimSequence>(WEP + TEXT("FP/A_TFA_FP_WEP_AR_Reload_Empty"));
	A.ReloadSoundMontage = Soft<UAnimMontage>(WEP + TEXT("FP/AM_TFA_FP_WEP_AR_Reload"));
	A.ReloadEmptySoundMontage = Soft<UAnimMontage>(WEP + TEXT("FP/AM_TFA_FP_WEP_AR_Reload_Empty"));

	A.FireSounds = { Soft<USoundBase>(AUD + TEXT("Firing/A_TFA_AR_Fire_Single_Cue")) };
	A.FireTail = Soft<USoundBase>(AUD + TEXT("Firing/A_TFA_AR_Fire_Tail_Short_Cue"));
	A.DryFire = Soft<USoundBase>(AUD + TEXT("Firing/A_TFA_AR_Fire_Empty_Cue"));
	A.EquipSound = Soft<USoundBase>(AUD + TEXT("Handling/Foley/A_TFA_AR_Handling_Cue"));

	A.MuzzleFlash = Soft<UNiagaraSystem>(TFA + TEXT("/Common/VFX/Systems/NS_TFA_MuzzleFlash"));
	A.MuzzleSocket = TEXT("SOCKET_Muzzle");

	// ---- Fallback (pack missing): grey boxes, code-posed arms ----
	S.HipOffset = FVector(45.f, 10.f, -26.f);
	S.SightPoint = FVector(-12.f, 0.f, 9.f);
	S.MuzzlePoint = FVector(40.f, 0.f, 6.f);
	S.RightHand.Location = FVector(-18.f, 3.5f, 1.f);
	S.RightHand.FingerDirection = FVector(0.85f, -0.1f, -0.5f);
	S.RightHand.PalmNormal = FVector(0.f, -1.f, 0.f);
	S.LeftHand.Location = FVector(14.f, -4.5f, 2.f);
	S.LeftHand.FingerDirection = FVector(0.85f, 0.5f, -0.15f);
	S.LeftHand.PalmNormal = FVector(0.f, 1.f, 0.f);
	S.LeftHand.FingerCurl = 70.f;
	const FLinearColor Dark(0.05f, 0.05f, 0.055f);
	S.Parts.Add(MakeBoxPart(TEXT("Receiver"), FVector(-25.f, -2.f, 2.f), FVector(20.f, 2.f, 9.f), Dark));
	S.Parts.Add(MakeBoxPart(TEXT("Barrel"), FVector(20.f, -1.f, 4.f), FVector(40.f, 1.f, 6.f), Dark));
	S.Parts.Add(MakeBoxPart(TEXT("Stock"), FVector(-45.f, -2.f, 0.f), FVector(-25.f, 2.f, 8.f), Dark));
	S.Parts.Add(MakeBoxPart(TEXT("Magazine"), FVector(-4.f, -1.5f, -12.f), FVector(4.f, 1.5f, 2.f), Dark, true));
	S.Parts.Add(MakeBoxPart(TEXT("Grip"), FVector(-21.f, -1.5f, -7.f), FVector(-16.f, 1.5f, 2.f), Dark));
}
