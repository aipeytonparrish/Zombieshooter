#include "Weapons/ZSWeapon_RD704.h"

AZSWeapon_RD704::AZSWeapon_RD704()
{
	FZSWeaponStats& S = Stats;
	S.DisplayName = NSLOCTEXT("ZS", "RD704", "RD-704");
	S.WeaponClass = EZSWeaponClass::AssaultRifle;

	S.Damage = 34.f;
	S.HeadshotMultiplier = 2.f;
	S.LimbMultiplier = 0.8f;
	S.FalloffStart = 2500.f;
	S.FalloffEnd = 6000.f;
	S.MinDamageMultiplier = 0.65f;
	S.PenetrationDepth = 25.f;
	S.MaxPenetrations = 2;

	S.FireRateRPM = 650.f;
	S.FireModes = { EZSFireMode::Auto, EZSFireMode::Burst, EZSFireMode::Semi };
	S.BurstCount = 3;

	S.MagSize = 30;
	S.MaxReserve = 180;
	S.TacticalReloadTime = 2.1f;
	S.EmptyReloadTime = 2.7f;

	// 30-shot pattern: strong first kicks, then a steady climb that drifts right, then left.
	S.RecoilPattern.Reset();
	for (int32 i = 0; i < 30; ++i)
	{
		const float Pitch = i < 4 ? 0.5f : 0.3f;
		const float Yaw = i < 8 ? 0.06f : (i < 18 ? 0.16f : -0.18f);
		S.RecoilPattern.Add(FVector2D(Yaw, Pitch));
	}
	S.RecoilRandomYaw = 0.12f;
	S.RecoilADSScale = 0.72f;
	S.RecoilRecoveryRate = 11.f;

	S.HipSpread = 2.2f;
	S.ADSSpread = 0.08f;
	S.MoveSpread = 1.6f;
	S.AirSpread = 3.5f;
	S.SpreadPerShot = 0.28f;
	S.MaxBloom = 2.4f;

	S.ADSTime = 0.28f;
	S.ADSFOVMultiplier = 0.62f;   // Bravo4 optic
	S.ADSMoveSpeedMultiplier = 0.72f;
	S.EquipTime = 0.6f;

	// ---- View model (weapon space: X = barrel, Y = right, Z = up; units cm) ----
	S.HipOffset = FVector(47.f, 10.f, -28.f);
	S.SightPoint = FVector(-19.5f, 0.f, 19.8f);   // optic centre line, rear of the scope
	S.ADSEyeRelief = 13.f;
	S.MuzzlePoint = FVector(34.7f, 0.f, 11.8f);
	S.VisualKickBack = 2.2f;
	S.VisualKickPitch = 1.6f;

	S.RightHand.Location = FVector(-26.f, 3.5f, 7.f);
	S.RightHand.FingerDirection = FVector(0.85f, -0.1f, -0.5f);
	S.RightHand.PalmNormal = FVector(0.f, -1.f, 0.f);
	S.RightHand.FingerCurl = 60.f;
	S.RightHand.IndexCurl = 25.f;
	S.RightHand.ThumbCurl = 25.f;
	S.RightHand.ElbowHint = FVector(-20.f, 25.f, -40.f);

	S.LeftHand.Location = FVector(9.5f, -4.5f, 6.f);
	S.LeftHand.FingerDirection = FVector(0.85f, 0.5f, -0.15f);
	S.LeftHand.PalmNormal = FVector(0.f, 1.f, 0.f);
	S.LeftHand.FingerCurl = 70.f;
	S.LeftHand.IndexCurl = 65.f;
	S.LeftHand.ThumbCurl = 40.f;
	S.LeftHand.ElbowHint = FVector(-20.f, -20.f, -45.f);

	// ---- Parts: modelled in place along +Y, so rotate yaw -90 to put the barrel on +X ----
	const FTransform ToWeaponSpace(FRotator(0.f, -90.f, 0.f));
	const FLinearColor Dark(0.04f, 0.04f, 0.045f);
	const FLinearColor Tan(0.25f, 0.2f, 0.13f);

	auto AddPart = [&](const TCHAR* Name, bool bMag, const FVector& BoxMin, const FVector& BoxMax, const FLinearColor& Color)
	{
		FZSWeaponPart P = MakeBoxPart(FName(Name), BoxMin, BoxMax, Color, bMag);
		P.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("/Game/ZS_Imported/Weapons/RD704/SM_RD704_%s.SM_RD704_%s"), Name, Name)));
		P.Offset = ToWeaponSpace;
		S.Parts.Add(P);
	};

	AddPart(TEXT("Receiver"),    false, FVector(-22.f, -2.f, 7.f),   FVector(22.f, 2.f, 15.f), Dark);
	AddPart(TEXT("Handguard"),   false, FVector(3.f, -2.2f, 8.5f),   FVector(27.f, 2.2f, 15.8f), Dark);
	AddPart(TEXT("Cover"),       false, FVector(-23.f, -1.7f, 12.4f), FVector(1.f, 1.7f, 16.5f), Dark);
	AddPart(TEXT("Grip"),        false, FVector(-22.f, -1.5f, -1.f), FVector(-16.f, 1.5f, 8.f), Tan);
	AddPart(TEXT("Stock"),       false, FVector(-45.f, -2.f, 2.f),   FVector(-21.f, 2.f, 12.f), Tan);
	AddPart(TEXT("Magazine"),    true,  FVector(-7.f, -1.5f, -8.f),  FVector(4.f, 1.5f, 9.f), Tan);
	AddPart(TEXT("MagAmmo"),     true,  FVector(-1.f, -1.f, 8.f),    FVector(1.f, 1.f, 9.f), Tan);
	AddPart(TEXT("Scope"),       false, FVector(-20.5f, -2.5f, 16.5f), FVector(-6.f, 2.5f, 22.f), Dark);
	AddPart(TEXT("Reticle"),     false, FVector(-19.6f, -0.2f, 19.6f), FVector(-19.4f, 0.2f, 20.f), FLinearColor::Red);
	AddPart(TEXT("ForeGrip"),    false, FVector(14.6f, -1.5f, 2.6f), FVector(19.2f, 1.5f, 9.f), Dark);
	AddPart(TEXT("MuzzleBrake"), false, FVector(29.f, -1.6f, 10.2f), FVector(34.7f, 1.6f, 13.4f), Dark);
}
