#include "UI/ZSHUD.h"
#include "Weapons/ZSWeaponComponent.h"
#include "Weapons/ZSWeaponBase.h"
#include "Player/ZSPlayerCharacter.h"
#include "Core/ZSGameUserSettings.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FLinearColor HudWhite(0.95f, 0.95f, 0.95f, 0.9f);
	const FLinearColor HudShadow(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor KillRed(1.f, 0.15f, 0.1f, 1.f);
	const FLinearColor HeadGold(1.f, 0.8f, 0.2f, 1.f);

	FText FireModeText(EZSFireMode Mode)
	{
		switch (Mode)
		{
		case EZSFireMode::Auto:  return NSLOCTEXT("ZS", "Auto", "AUTO");
		case EZSFireMode::Burst: return NSLOCTEXT("ZS", "Burst", "BURST");
		default:                 return NSLOCTEXT("ZS", "Semi", "SEMI");
		}
	}
}

void AZSHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	const AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(GetOwningPawn());
	UZSWeaponComponent* Weapons = Char ? Char->GetWeaponComponent() : nullptr;
	if (!Weapons || !Weapons->GetCurrentWeapon())
	{
		return;
	}

	// Everything is authored for 1080p and scaled to the actual resolution.
	const float Scale = Canvas->ClipY / 1080.f;
	DrawCrosshair(Weapons, Scale);
	DrawHitMarkers(Weapons, Scale);
	DrawDamageNumbers(Weapons, Scale);
	DrawAmmo(Weapons, Scale);
}

void AZSHUD::DrawCrosshair(UZSWeaponComponent* Weapons, float Scale)
{
	const float ADS = Weapons->GetADSAlpha();
	if (ADS > 0.6f || Weapons->IsWeaponLowered())
	{
		return;
	}

	const APlayerController* PC = GetOwningPlayerController();
	const float FOV = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : 80.f;
	const float HalfW = Canvas->ClipX * 0.5f;
	const float HalfH = Canvas->ClipY * 0.5f;

	// Spread angle -> pixels on screen.
	const float SpreadPx = FMath::Tan(FMath::DegreesToRadians(Weapons->GetCurrentSpread())) / FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f)) * HalfW;
	const float Gap = FMath::Max(SpreadPx, 4.f * Scale);
	const float Len = 11.f * Scale;
	const float Thick = 2.f * Scale;
	FLinearColor Color = HudWhite;
	Color.A *= 1.f - ADS / 0.6f;

	auto Line = [this, Thick](float X1, float Y1, float X2, float Y2, const FLinearColor& C)
	{
		DrawLine(X1 + 1.f, Y1 + 1.f, X2 + 1.f, Y2 + 1.f, HudShadow * FLinearColor(1, 1, 1, C.A), Thick + 1.f);
		DrawLine(X1, Y1, X2, Y2, C, Thick);
	};
	Line(HalfW - Gap - Len, HalfH, HalfW - Gap, HalfH, Color);
	Line(HalfW + Gap, HalfH, HalfW + Gap + Len, HalfH, Color);
	Line(HalfW, HalfH - Gap - Len, HalfW, HalfH - Gap, Color);
	Line(HalfW, HalfH + Gap, HalfW, HalfH + Gap + Len, Color);
	DrawRect(Color, HalfW - Thick * 0.5f, HalfH - Thick * 0.5f, Thick, Thick);
}

void AZSHUD::DrawHitMarkers(UZSWeaponComponent* Weapons, float Scale)
{
	const UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	const float Age = Now - Weapons->GetLastHitTime();
	const float Duration = 0.22f;
	if (Age > Duration)
	{
		return;
	}

	bool bKill = false, bHead = false;
	for (const FZSHitInfo& H : Weapons->GetRecentHits())
	{
		if (Now - H.Time <= Duration)
		{
			bKill |= H.bKill;
			bHead |= H.bHeadshot;
		}
	}

	const float Alpha = 1.f - Age / Duration;
	const float Pop = 1.f + 0.4f * Alpha;
	const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f;
	const float Inner = 7.f * Scale * Pop, Outer = (bKill ? 19.f : 15.f) * Scale * Pop;
	FLinearColor Color = bKill ? KillRed : (bHead ? HeadGold : HudWhite);
	Color.A = Alpha;
	const float Thick = (bKill ? 3.5f : 2.5f) * Scale;

	for (int32 i = 0; i < 4; ++i)
	{
		const float SX = (i & 1) ? 1.f : -1.f;
		const float SY = (i & 2) ? 1.f : -1.f;
		DrawLine(CX + SX * Inner, CY + SY * Inner, CX + SX * Outer, CY + SY * Outer, Color, Thick);
	}
}

void AZSHUD::DrawDamageNumbers(UZSWeaponComponent* Weapons, float Scale)
{
	if (!UZSGameUserSettings::Get()->bShowDamageNumbers)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	UFont* Font = GEngine->GetMediumFont();
	for (const FZSHitInfo& H : Weapons->GetRecentHits())
	{
		const float Age = Now - H.Time;
		if (Age > 0.9f)
		{
			continue;
		}
		const FVector Screen = Project(H.Location + FVector(0.f, 0.f, 25.f + Age * 60.f));
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		FLinearColor Color = H.bKill ? KillRed : (H.bHeadshot ? HeadGold : HudWhite);
		Color.A = 1.f - Age / 0.9f;
		const FString Text = FString::Printf(TEXT("%d"), FMath::RoundToInt(H.Damage));
		const float TextScale = (H.bHeadshot ? 1.6f : 1.25f) * Scale;
		DrawText(Text, HudShadow * FLinearColor(1, 1, 1, Color.A), Screen.X + 2.f, Screen.Y + 2.f, Font, TextScale);
		DrawText(Text, Color, Screen.X, Screen.Y, Font, TextScale);
	}
}

void AZSHUD::DrawAmmo(UZSWeaponComponent* Weapons, float Scale)
{
	const AZSWeaponBase* W = Weapons->GetCurrentWeapon();
	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetMediumFont();

	const float Right = Canvas->ClipX - 70.f * Scale;
	const float Bottom = Canvas->ClipY - 70.f * Scale;

	const int32 Mag = W->GetAmmoInMag();
	const FLinearColor MagColor = Mag == 0 ? KillRed : (Mag <= W->Stats.MagSize / 4 ? HeadGold : HudWhite);
	const FString MagText = FString::Printf(TEXT("%d"), Mag);
	const FString ReserveText = FString::Printf(TEXT("/ %d"), W->GetReserveAmmo());

	float MagW, MagH, ResW, ResH;
	GetTextSize(MagText, MagW, MagH, Big, 2.6f * Scale);
	GetTextSize(ReserveText, ResW, ResH, Small, 1.6f * Scale);

	const float ResX = Right - ResW;
	const float MagX = ResX - MagW - 10.f * Scale;
	DrawText(MagText, HudShadow, MagX + 2.f, Bottom - MagH + 2.f, Big, 2.6f * Scale);
	DrawText(MagText, MagColor, MagX, Bottom - MagH, Big, 2.6f * Scale);
	DrawText(ReserveText, HudWhite, ResX, Bottom - ResH - 6.f * Scale, Small, 1.6f * Scale);

	const FString Info = FString::Printf(TEXT("%s   %s"), *W->Stats.DisplayName.ToString().ToUpper(), *FireModeText(W->GetFireMode()).ToString());
	float InfoW, InfoH;
	GetTextSize(Info, InfoW, InfoH, Small, 1.2f * Scale);
	DrawText(Info, HudShadow, Right - InfoW + 2.f, Bottom - MagH - InfoH - 4.f * Scale + 2.f, Small, 1.2f * Scale);
	DrawText(Info, FLinearColor(0.8f, 0.8f, 0.8f, 0.9f), Right - InfoW, Bottom - MagH - InfoH - 4.f * Scale, Small, 1.2f * Scale);

	// Reload bar under the crosshair.
	const float Progress = Weapons->GetReloadProgress();
	if (Progress > 0.f)
	{
		const float BarW = 160.f * Scale, BarH = 6.f * Scale;
		const float X = Canvas->ClipX * 0.5f - BarW * 0.5f, Y = Canvas->ClipY * 0.5f + 60.f * Scale;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X, Y, BarW, BarH);
		DrawRect(HudWhite, X, Y, BarW * Progress, BarH);
		const FString Label = TEXT("RELOADING");
		float LW, LH;
		GetTextSize(Label, LW, LH, Small, 1.1f * Scale);
		DrawText(Label, HudWhite, Canvas->ClipX * 0.5f - LW * 0.5f, Y + BarH + 4.f * Scale, Small, 1.1f * Scale);
	}
	else if (Mag == 0 && W->GetReserveAmmo() == 0)
	{
		const FString Label = TEXT("NO AMMO");
		float LW, LH;
		GetTextSize(Label, LW, LH, Small, 1.3f * Scale);
		DrawText(Label, KillRed, Canvas->ClipX * 0.5f - LW * 0.5f, Canvas->ClipY * 0.5f + 60.f * Scale, Small, 1.3f * Scale);
	}
}
