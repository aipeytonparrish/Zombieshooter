#include "Weapons/ZSWeapon_Pistol.h"
#include "Weapons/ZSAssetPath.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

using ZSAssets::Soft;

AZSWeapon_Pistol::AZSWeapon_Pistol()
{
	FZSWeaponStats& S = Stats;
	S.DisplayName = NSLOCTEXT("ZS", "Pistol", "P9 Sidearm");
	S.WeaponClass = EZSWeaponClass::Pistol;

	S.Damage = 28.f;
	S.HeadshotMultiplier = 2.2f;
	S.LimbMultiplier = 0.85f;
	S.FalloffStart = 1500.f;
	S.FalloffEnd = 3500.f;
	S.MinDamageMultiplier = 0.5f;
	S.PenetrationDepth = 8.f;
	S.MaxPenetrations = 1;

	S.FireRateRPM = 420.f;
	S.SemiMaxRPM = 420.f;
	S.FireModes = { EZSFireMode::Semi };

	S.MagSize = 12;
	S.MaxReserve = 72;
	S.TacticalReloadTime = 1.4f;
	S.EmptyReloadTime = 1.8f;

	S.RecoilPattern = { FVector2D(0.05f, 1.1f), FVector2D(-0.05f, 1.f), FVector2D(0.08f, 1.05f) };
	S.RecoilRandomYaw = 0.25f;
	S.RecoilADSScale = 0.8f;
	S.RecoilApplySpeed = 30.f;
	S.RecoilRecoveryRate = 14.f;

	S.HipSpread = 1.6f;
	S.ADSSpread = 0.15f;
	S.MoveSpread = 1.0f;
	S.AirSpread = 2.5f;
	S.SpreadPerShot = 0.45f;
	S.MaxBloom = 2.f;
	S.BloomRecovery = 7.f;

	S.ADSTime = 0.18f;
	S.ADSFOVMultiplier = 0.85f;
	S.ADSMoveSpeedMultiplier = 0.85f;
	S.EquipTime = 0.35f;
	S.UnequipTime = 0.2f;
	S.SprintToFireTime = 0.1f;

	// ---- View model ----
	S.HipOffset = FVector(32.f, 8.f, -14.f);
	S.SightPoint = FVector(-3.f, 0.f, 7.0f);
	S.ADSEyeRelief = 44.f;
	S.MuzzlePoint = FVector(15.5f, 0.f, 5.f);
	S.VisualKickBack = 3.f;
	S.VisualKickPitch = 5.f;

	S.RightHand.Location = FVector(-8.f, 3.f, 0.f);
	S.RightHand.FingerDirection = FVector(0.8f, -0.15f, -0.55f);
	S.RightHand.PalmNormal = FVector(0.f, -1.f, 0.f);
	S.RightHand.FingerCurl = 65.f;
	S.RightHand.IndexCurl = 25.f;
	S.RightHand.ThumbCurl = 20.f;
	S.RightHand.ElbowHint = FVector(-20.f, 20.f, -40.f);

	S.LeftHand.Location = FVector(-7.f, -4.5f, -5.f);
	S.LeftHand.FingerDirection = FVector(0.7f, 0.6f, 0.2f);
	S.LeftHand.PalmNormal = FVector(0.f, 0.7f, 0.7f);
	S.LeftHand.FingerCurl = 60.f;
	S.LeftHand.IndexCurl = 55.f;
	S.LeftHand.ThumbCurl = 15.f;
	S.LeftHand.ElbowHint = FVector(-20.f, -25.f, -40.f);

	// ---- Animated rig (ProFPS Animation Bundle: Pistol V2) ----
	{
		const FString P = TEXT("/Game/ProFPSAnimationBundleV2/PistolAnimationV2");
		const FString An = P + TEXT("/Animations/");
		const FString Snd = TEXT("/Game/RealWeaponSoundFX/WeaponPistol001/Cues/");
		FZSWeaponAnimSet& A = AnimSet;
		A.DriverMesh = Soft<USkeletalMesh>(P + TEXT("/Demo/FirstPersonArms/Character/Mesh/SK_Mannequin_Arms"));
		A.WeaponMesh = Soft<USkeletalMesh>(P + TEXT("/Meshes/Pistol/SKM_PistolV2"));
		// Placement copied from the pack's BP_DemoCharacter (hands relative to the camera, pistol on hand_r).
		A.WeaponAttachBone = TEXT("hand_r");
		A.WeaponAttachOffset = FTransform(FRotator(17.6f, 81.f, 6.6f), FVector(-17.42f, 4.47f, 2.14f));
		A.DriverRelativeToCamera = FTransform(FRotator(0.f, -90.f, 0.f), FVector(15.f, 0.f, -162.61f));

		A.Idle = Soft<UAnimSequence>(An + TEXT("AS_Player_PistolV2_Idle"));
		A.Walk = Soft<UAnimSequence>(An + TEXT("AS_Player_PistolV2_Walk"));
		A.Sprint = Soft<UAnimSequence>(An + TEXT("AS_Player_PistolV2_Sprint"));
		A.AimIdle = Soft<UAnimSequence>(An + TEXT("AS_Player_PistolV2_ADS_Loop"));
		A.AimWalk = A.AimIdle;
		A.Fire = Soft<UAnimSequence>(An + TEXT("AS_Player_PistolV2_Fire"));
		A.FireAimed = Soft<UAnimSequence>(An + TEXT("AS_Player_PistolV2_ADS_Fire"));
		A.Reload = Soft<UAnimSequence>(An + TEXT("AS_Player_PistolV2_Reload"));
		A.ReloadEmpty = Soft<UAnimSequence>(An + TEXT("AS_Player_PistolV2_Reload_Empty"));
		A.Equip = Soft<UAnimSequence>(An + TEXT("AS_Player_PistolV2_Draw"));
		A.Holster = Soft<UAnimSequence>(An + TEXT("AS_Player_PistolV2_Holster"));

		A.WeaponIdle = Soft<UAnimSequence>(An + TEXT("PistolV2/AS_SK_PistolV2_Idle"));
		A.WeaponFire = Soft<UAnimSequence>(An + TEXT("PistolV2/AS_SK_PistolV2_Fire"));
		A.WeaponReload = Soft<UAnimSequence>(An + TEXT("PistolV2/AS_SK_PistolV2_Reload"));
		A.WeaponReloadEmpty = Soft<UAnimSequence>(An + TEXT("PistolV2/AS_SK_PistolV2_Reload_Empty"));

		for (int32 i = 1; i <= 5; ++i)
		{
			A.FireSounds.Add(Soft<USoundBase>(Snd + FString::Printf(TEXT("Cue_WpnPistol001_Shot_Single_0%d"), i)));
		}
		A.DryFire = Soft<USoundBase>(TEXT("/Game/GunSoundsPackVol1/Foley/cue/Dry_Fire_1-1_Cue"));
		A.EquipSound = Soft<USoundBase>(Snd + TEXT("Cue_WpnPistol001_Equip"));
		// The pack's clips carry no sound events, so the reload sounds are timed here.
		auto Timed = [](float Fraction, const TSoftObjectPtr<USoundBase>& Sound) { FZSTimedSound T; T.Fraction = Fraction; T.Sound = Sound; return T; };
		A.ReloadSounds = { Timed(0.2f, Soft<USoundBase>(Snd + TEXT("Cue_WpnPistol001_Clip_Eject"))), Timed(0.55f, Soft<USoundBase>(Snd + TEXT("Cue_WpnPistol001_Clip_Insert"))) };
		A.ReloadEmptySounds = { Timed(0.18f, Soft<USoundBase>(Snd + TEXT("Cue_WpnPistol001_Clip_Eject"))), Timed(0.5f, Soft<USoundBase>(Snd + TEXT("Cue_WpnPistol001_Clip_Insert"))),
			Timed(0.78f, Soft<USoundBase>(TEXT("/Game/GunSoundsPackVol1/Foley/cue/Hammer_Back_1-1_Cue"))) };

		A.MuzzleFlash = Soft<UNiagaraSystem>(TEXT("/Game/InfimaGames/TacticalFPSAnimations/Common/VFX/Systems/NS_TFA_MuzzleFlash"));
		A.MuzzleOffset = FVector(0.f, 11.f, 2.5f);

		S.TacticalReloadTime = 1.75f;
		S.EmptyReloadTime = 2.05f;
		S.EquipTime = 0.6f;
		S.UnequipTime = 0.4f;
	}

	// ---- Placeholder geometry ----
	const FLinearColor Slide(0.06f, 0.06f, 0.065f);
	const FLinearColor Frame(0.02f, 0.02f, 0.02f);
	const FLinearColor Sight(0.9f, 0.9f, 0.9f);

	S.Parts.Add(MakeBoxPart(TEXT("Slide"), FVector(-4.f, -1.4f, 3.f), FVector(15.f, 1.4f, 6.5f), Slide));
	S.Parts.Add(MakeBoxPart(TEXT("Frame"), FVector(-2.f, -1.3f, 0.5f), FVector(13.f, 1.3f, 3.f), Frame));
	S.Parts.Add(MakeBoxPart(TEXT("Grip"), FVector(-4.5f, -1.5f, -9.f), FVector(1.5f, 1.5f, 1.f), Frame));
	S.Parts.Add(MakeBoxPart(TEXT("Magazine"), FVector(-4.f, -1.2f, -10.f), FVector(1.f, 1.2f, -8.5f), Slide, true));
	S.Parts.Add(MakeBoxPart(TEXT("TriggerGuard"), FVector(1.5f, -0.5f, -1.8f), FVector(5.5f, 0.5f, -1.2f), Frame));
	S.Parts.Add(MakeCylinderPart(TEXT("Barrel"), FVector(15.2f, 0.f, 5.f), 1.f, 1.1f, Frame));
	S.Parts.Add(MakeBoxPart(TEXT("RearSight"), FVector(-3.5f, -1.f, 6.5f), FVector(-2.5f, 1.f, 7.3f), Sight));
	S.Parts.Add(MakeBoxPart(TEXT("FrontSight"), FVector(13.5f, -0.25f, 6.5f), FVector(14.5f, 0.25f, 7.3f), Sight));
}
