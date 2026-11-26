#include "Weapons/ZSWeapon_Pistol.h"

AZSWeapon_Pistol::AZSWeapon_Pistol()
{
	FZSWeaponStats& S = Stats;
	S.DisplayName = NSLOCTEXT("ZS", "Pistol", "M9 Sidearm");
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
