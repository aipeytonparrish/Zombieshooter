#include "UI/ZSHUD.h"
#include "Weapons/ZSWeaponComponent.h"
#include "Weapons/ZSWeaponBase.h"
#include "Player/ZSPlayerCharacter.h"
#include "Core/ZSGameUserSettings.h"
#include "Combat/ZSHealthComponent.h"
#include "Rounds/ZSGameState_Survival.h"
#include "Interaction/ZSInteractionComponent.h"
#include "CommonInputSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Player/ZSPlayerController.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Bunker/ZSBunkerGameMode.h"
#include "Profile/ZSProfileSubsystem.h"
#include "Missions/ZSMissionSubsystem.h"
#include "Core/ZSCharacterCatalog.h"
#include "Engine/Texture2D.h"

namespace
{
	const FLinearColor HudWhite(0.95f, 0.95f, 0.95f, 0.9f);
	const FLinearColor HudShadow(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor KillRed(1.f, 0.15f, 0.1f, 1.f);
	const FLinearColor HeadGold(1.f, 0.8f, 0.2f, 1.f);

	// Left 4 Dead health colours.
	FLinearColor HealthColor(float Frac)
	{
		if (Frac > 0.4f) { return FLinearColor(0.36f, 0.85f, 0.3f, 1.f); }
		if (Frac > 0.24f) { return FLinearColor(0.96f, 0.8f, 0.2f, 1.f); }
		return FLinearColor(0.95f, 0.18f, 0.12f, 1.f);
	}
	const FLinearColor PlateTint(0.025f, 0.025f, 0.025f, 0.78f);
	const FLinearColor LabelColor(0.86f, 0.82f, 0.72f, 0.95f);

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
	// Everything is authored for 1080p and scaled to the actual resolution.
	const float Scale = Canvas->ClipY / 1080.f;
	const AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>();
	AZSBunkerGameMode* Bunker = GetWorld()->GetAuthGameMode<AZSBunkerGameMode>();
	if (Char && !Bunker)
	{
		DrawHealth(Char, Scale);
	}
	if (Bunker)
	{
		DrawBunker(Bunker, Scale);
	}
	if (Char)
	{
		DrawInteractPrompt(Char, Scale);
	}
	DrawToasts(Scale);
	{
		const float Now = GetWorld()->GetRealTimeSeconds();
		DrawScreenBlood(FMath::Clamp(Now - LastDrawTime, 0.f, 0.1f));
		LastDrawTime = Now;
	}
	if (GS)
	{
		DrawSurvival(GS, Char, Scale);
	}
	else
	{
		DrawPauseMenu(Scale);
	}
	UZSWeaponComponent* Weapons = Char ? Char->GetWeaponComponent() : nullptr;
	if (!Weapons || !Weapons->GetCurrentWeapon() || (Char->GetHealthComponent() && Char->GetHealthComponent()->IsDead()) || (GS && GS->bShowResults))
	{
		return;
	}
	DrawCrosshair(Weapons, Scale);
	DrawHitMarkers(Weapons, Scale);
	DrawDamageNumbers(Weapons, Scale);
	DrawAmmo(Weapons, Scale);
}

void AZSHUD::EnsureHudTextures()
{
	if (bTriedHudTextures)
	{
		return;
	}
	bTriedHudTextures = true;
	PlateTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/ZS/Textures/FX/T_ZS_HudPlate.T_ZS_HudPlate"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	SmearTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/ZS/Textures/FX/T_ZS_HudSmear.T_ZS_HudSmear"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void AZSHUD::DrawPlate(float X, float Y, float W, float H, const FLinearColor& Tint)
{
	EnsureHudTextures();
	if (PlateTex)
	{
		// A random-looking but stable window into the grunge texture, so plates don't all look identical.
		const float U = FMath::Frac(X * 0.0013f), V = FMath::Frac(Y * 0.0017f);
		DrawTexture(PlateTex, X, Y, W, H, U * 0.3f, V * 0.3f, 0.7f, 0.7f * FMath::Min(1.f, H / FMath::Max(W, 1.f)) + 0.3f, Tint, BLEND_Translucent);
	}
	else
	{
		DrawRect(Tint, X, Y, W, H);
	}
	// A thin worn rule along the top edge.
	DrawRect(FLinearColor(0.6f, 0.55f, 0.45f, 0.18f * Tint.A), X + 6.f, Y + 3.f, W - 12.f, 1.f);
}

void AZSHUD::DrawSmear(float X, float Y, float W, float H, float Alpha)
{
	EnsureHudTextures();
	if (SmearTex)
	{
		DrawTexture(SmearTex, X, Y, W, H, 0.f, 0.f, 1.f, 1.f, FLinearColor(0.55f, 0.5f, 0.5f, Alpha), BLEND_Translucent);
	}
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

	// Weapon card, bottom right: a grungy plate with the name, a big magazine count, reserve, and a row of rounds.
	const float CardW = 400.f * Scale, CardH = 118.f * Scale;
	const float CardX = Canvas->ClipX - 40.f * Scale - CardW, CardY = Canvas->ClipY - 36.f * Scale - CardH;
	DrawPlate(CardX, CardY, CardW, CardH, PlateTint);
	const float Right = CardX + CardW - 22.f * Scale;
	const float Bottom = CardY + CardH - 16.f * Scale;

	const int32 Mag = W->GetAmmoInMag();
	const int32 MagSize = FMath::Max(1, W->Stats.MagSize);
	const FLinearColor MagColor = Mag == 0 ? KillRed : (Mag <= MagSize / 4 ? HeadGold : FLinearColor(0.95f, 0.93f, 0.88f, 1.f));
	const FString MagText = FString::Printf(TEXT("%d"), Mag);
	const FString ReserveText = FString::Printf(TEXT("%d"), W->GetReserveAmmo());

	float MagW, MagH, ResW, ResH;
	GetTextSize(MagText, MagW, MagH, Big, 3.6f * Scale);
	GetTextSize(ReserveText, ResW, ResH, Small, 1.8f * Scale);
	const float ResX = Right - ResW;
	const float MagX = ResX - MagW - 18.f * Scale;
	const float MagY = Bottom - MagH + 10.f * Scale;
	DrawText(MagText, HudShadow, MagX + 3.f, MagY + 3.f, Big, 3.6f * Scale);
	DrawText(MagText, MagColor, MagX, MagY, Big, 3.6f * Scale);
	DrawRect(FLinearColor(0.7f, 0.65f, 0.55f, 0.5f), ResX - 10.f * Scale, Bottom - MagH * 0.75f, 2.f * Scale, MagH * 0.6f);   // divider
	DrawText(ReserveText, HudShadow, ResX + 2.f, Bottom - ResH - 4.f * Scale + 2.f, Small, 1.8f * Scale);
	DrawText(ReserveText, LabelColor, ResX, Bottom - ResH - 4.f * Scale, Small, 1.8f * Scale);

	const FString Name = W->Stats.DisplayName.ToString().ToUpper();
	DrawText(Name, HudShadow, CardX + 22.f * Scale + 2.f, CardY + 14.f * Scale + 2.f, Small, 1.25f * Scale);
	DrawText(Name, LabelColor, CardX + 22.f * Scale, CardY + 14.f * Scale, Small, 1.25f * Scale);
	const FString Mode = FireModeText(W->GetFireMode()).ToString();
	float ModeW, ModeH;
	GetTextSize(Mode, ModeW, ModeH, Small, 0.95f * Scale);
	DrawRect(FLinearColor(0.75f, 0.1f, 0.06f, 0.85f), Right - ModeW - 12.f * Scale, CardY + 14.f * Scale, ModeW + 12.f * Scale, ModeH + 2.f * Scale);
	DrawText(Mode, FLinearColor(1.f, 0.92f, 0.85f, 1.f), Right - ModeW - 6.f * Scale, CardY + 15.f * Scale, Small, 0.95f * Scale);

	// Rounds: one pip per round (grouped for big mags), spent ones dark.
	{
		const int32 Pips = FMath::Min(MagSize, 40);
		const float PerPip = float(MagSize) / Pips;
		const float PipArea = MagX - (CardX + 22.f * Scale) - 16.f * Scale;
		const float PipW = FMath::Clamp(PipArea / Pips - 2.f * Scale, 2.f * Scale, 7.f * Scale);
		const float PipH = 22.f * Scale;
		const float PipY = Bottom - PipH - 4.f * Scale;
		for (int32 i = 0; i < Pips; ++i)
		{
			const bool bFull = (i + 0.5f) * PerPip <= Mag;
			const float PX = CardX + 22.f * Scale + i * (PipW + 2.f * Scale);
			DrawRect(bFull ? FLinearColor(0.85f, 0.7f, 0.35f, 0.95f) : FLinearColor(0.2f, 0.18f, 0.15f, 0.7f), PX, PipY, PipW, PipH);
		}
	}

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

void AZSHUD::DrawHealth(const AZSPlayerCharacter* Char, float Scale)
{
	const UZSHealthComponent* Health = Char->GetHealthComponent();
	if (!Health)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const float Frac = FMath::Clamp(Health->GetHealth() / FMath::Max(Health->MaxHealth, 1.f), 0.f, 1.f);

	// Red screen edge: a flash when hit plus a slow pulse when low.
	const float Flash = FMath::Clamp(1.f - (Now - Char->GetLastDamageTime()) / 0.45f, 0.f, 1.f);
	const float Low = Frac < 0.35f ? (0.35f - Frac) / 0.35f * (0.6f + 0.4f * FMath::Sin(Now * 5.f)) : 0.f;
	const float EdgeAlpha = FMath::Clamp(Flash * 0.45f + Low * 0.35f, 0.f, 0.7f);
	if (EdgeAlpha > 0.01f)
	{
		const int32 Steps = 10;
		const float Band = Canvas->ClipY * 0.22f / Steps;
		for (int32 i = 0; i < Steps; ++i)
		{
			const float A = EdgeAlpha * (1.f - float(i) / Steps) * 0.35f;
			const FLinearColor C(0.55f, 0.f, 0.f, A);
			const float O = i * Band;
			DrawRect(C, O, O, Canvas->ClipX - 2.f * O, Band);                          // top
			DrawRect(C, O, Canvas->ClipY - O - Band, Canvas->ClipX - 2.f * O, Band);   // bottom
			DrawRect(C, O, O + Band, Band, Canvas->ClipY - 2.f * O - 2.f * Band);      // left
			DrawRect(C, Canvas->ClipX - O - Band, O + Band, Band, Canvas->ClipY - 2.f * O - 2.f * Band); // right
		}
	}

	// Survivor card, bottom left (Left 4 Dead style): name, big health number and bar in green / yellow / red,
	// a white trail showing damage just taken, armour plates underneath.
	const float CardW = 440.f * Scale, CardH = 118.f * Scale;
	const float CardX = 40.f * Scale, CardY = Canvas->ClipY - 36.f * Scale - CardH;
	DrawPlate(CardX, CardY, CardW, CardH, PlateTint);
	UFont* Small = GEngine->GetMediumFont();
	UFont* BigFont = GEngine->GetLargeFont();
	const FLinearColor HC = HealthColor(Frac);

	FString Name = TEXT("SURVIVOR");
	if (const UZSProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UZSProfileSubsystem>() : nullptr)
	{
		const TArray<ZSCharacterCatalog::FBody>& Bodies = ZSCharacterCatalog::Bodies();
		if (Bodies.IsValidIndex(Profile->GetLook().Body))
		{
			Name = Bodies[Profile->GetLook().Body].DisplayName.ToString().ToUpper();
		}
		Name += FString::Printf(TEXT("   LV %d"), Profile->GetLevel());
	}
	DrawText(Name, HudShadow, CardX + 22.f * Scale + 2.f, CardY + 14.f * Scale + 2.f, Small, 1.15f * Scale);
	DrawText(Name, LabelColor, CardX + 22.f * Scale, CardY + 14.f * Scale, Small, 1.15f * Scale);

	const FString HPText = FString::Printf(TEXT("%d"), FMath::CeilToInt(Health->GetHealth()));
	float HPW, HPH;
	GetTextSize(HPText, HPW, HPH, BigFont, 3.4f * Scale);
	const float HPX = CardX + 22.f * Scale, HPY = CardY + CardH - HPH - 4.f * Scale;
	const float Pulse = Frac <= 0.24f ? 0.65f + 0.35f * FMath::Sin(Now * 7.f) : 1.f;
	DrawText(HPText, HudShadow, HPX + 3.f, HPY + 3.f, BigFont, 3.4f * Scale);
	DrawText(HPText, HC * FLinearColor(1.f, 1.f, 1.f, Pulse), HPX, HPY, BigFont, 3.4f * Scale);

	// Damage trail: holds for a moment after a hit, then drains down to the real value.
	const float Real = Health->GetHealth();
	if (HealthTrail < 0.f) { HealthTrail = Real; LastHealthSeen = Real; }
	if (Real < LastHealthSeen) { HealthTrailHold = 0.5f; }
	LastHealthSeen = Real;
	const float Dt = FMath::Clamp(GetWorld()->GetDeltaSeconds(), 0.f, 0.1f);
	if (HealthTrailHold > 0.f) { HealthTrailHold -= Dt; }
	else { HealthTrail = FMath::FInterpConstantTo(HealthTrail, Real, Dt, Health->MaxHealth * 0.6f); }
	HealthTrail = FMath::Max(HealthTrail, Real);
	const float TrailFrac = FMath::Clamp(HealthTrail / FMath::Max(Health->MaxHealth, 1.f), 0.f, 1.f);

	const float W = CardW - (HPX - CardX) - 130.f * Scale - 22.f * Scale, H = 16.f * Scale;
	const float X = HPX + 130.f * Scale, Y = CardY + CardH - 50.f * Scale;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), X - 2.f, Y - 2.f, W + 4.f, H + 4.f);
	DrawRect(FLinearColor(0.08f, 0.07f, 0.06f, 0.9f), X, Y, W, H);
	DrawRect(FLinearColor(0.9f, 0.88f, 0.85f, 0.8f), X, Y, W * TrailFrac, H);
	DrawRect(HC * FLinearColor(0.8f, 0.8f, 0.8f, 1.f), X, Y, W * Frac, H);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.18f), X, Y, W * Frac, H * 0.35f);   // sheen
	for (int32 i = 1; i < 10; ++i)   // segment ticks
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X + W * i / 10.f, Y, 1.f, H);
	}
	if (Frac <= 0.24f)
	{
		const FString Warn = TEXT("BLEEDING OUT");
		DrawText(Warn, FLinearColor(0.95f, 0.2f, 0.12f, Pulse), X, Y - 26.f * Scale, Small, 1.05f * Scale);
	}

	// Armor plates: one segment per 50 points, above the health bar.
	if (Health->MaxArmor > 0.f)
	{
		const int32 Plates = FMath::Max(1, FMath::RoundToInt(Health->MaxArmor / 50.f));
		const float Gap = 6.f * Scale;
		const float SegW = (W - Gap * (Plates - 1)) / Plates;
		const float SegH = 7.f * Scale;
		const float SegY = Y + H + 8.f * Scale;
		const float SegX0 = X;
		for (int32 i = 0; i < Plates; ++i)
		{
			const float Fill = FMath::Clamp((Health->GetArmor() - i * 50.f) / 50.f, 0.f, 1.f);
			const float SX = SegX0 + i * (SegW + Gap);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), SX - 1.f, SegY - 1.f, SegW + 2.f, SegH + 2.f);
			DrawRect(FLinearColor(0.25f, 0.6f, 1.f, 0.95f), SX, SegY, SegW * Fill, SegH);
		}
	}

	if (Health->IsDead() && !GetWorld()->GetGameState<AZSGameState_Survival>())
	{
		DrawRect(FLinearColor(0.15f, 0.f, 0.f, 0.45f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		const FString Text = TEXT("YOU WERE KILLED");
		float TW, TH;
		UFont* Big = GEngine->GetLargeFont();
		GetTextSize(Text, TW, TH, Big, 4.f * Scale);
		DrawText(Text, KillRed, Canvas->ClipX * 0.5f - TW * 0.5f, Canvas->ClipY * 0.38f, Big, 4.f * Scale);
	}
}

// ---- Survival / interaction / toasts ---------------------------------------------------------

void AZSHUD::Toast(const APawn* Player, const FText& Message)
{
	if (const APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr)
	{
		if (AZSHUD* HUD = Cast<AZSHUD>(PC->GetHUD()))
		{
			HUD->AddToast(Message);
		}
	}
}

void AZSHUD::AddToast(const FText& Message)
{
	FToast& T = Toasts.AddDefaulted_GetRef();
	T.Text = Message;
	T.Time = GetWorld()->GetTimeSeconds();
	if (Toasts.Num() > 3)
	{
		Toasts.RemoveAt(0);
	}
}

void AZSHUD::DrawCentered(const FString& Text, float Y, UFont* Font, float TextScale, const FLinearColor& Color)
{
	float TW, TH;
	GetTextSize(Text, TW, TH, Font, TextScale);
	const float X = Canvas->ClipX * 0.5f - TW * 0.5f;
	DrawText(Text, HudShadow * FLinearColor(1, 1, 1, Color.A), X + 2.f, Y + 2.f, Font, TextScale);
	DrawText(Text, Color, X, Y, Font, TextScale);
}

void AZSHUD::DrawToasts(float Scale)
{
	const float Now = GetWorld()->GetTimeSeconds();
	Toasts.RemoveAll([Now](const FToast& T) { return Now - T.Time > 1.8f; });
	UFont* Font = GEngine->GetMediumFont();
	float Y = Canvas->ClipY * 0.72f;
	for (int32 i = Toasts.Num() - 1; i >= 0; --i)
	{
		const float Age = Now - Toasts[i].Time;
		FLinearColor C = HudWhite;
		C.A = FMath::Clamp((1.8f - Age) / 0.5f, 0.f, 1.f);
		DrawCentered(Toasts[i].Text.ToString(), Y, Font, 1.4f * Scale, C);
		Y -= 30.f * Scale;
	}
}

void AZSHUD::DrawInteractPrompt(const AZSPlayerCharacter* Char, float Scale)
{
	const UZSInteractionComponent* Interaction = Char->GetInteraction();
	if (!Interaction || !Interaction->GetFocused())
	{
		return;
	}
	bool bGamepad = false;
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		if (const UCommonInputSubsystem* Input = PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UCommonInputSubsystem>() : nullptr)
		{
			bGamepad = Input->GetCurrentInputType() == ECommonInputType::Gamepad;
		}
	}
	const FString Text = FString::Printf(TEXT("[%s]  %s"), bGamepad ? TEXT("X") : TEXT("E"), *Interaction->GetPrompt().ToString());
	DrawCentered(Text, Canvas->ClipY * 0.58f, GEngine->GetMediumFont(), 1.5f * Scale, HeadGold);
}

void AZSHUD::DrawSurvival(const AZSGameState_Survival* GS, const AZSPlayerCharacter* Char, float Scale)
{
	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetMediumFont();
	const float Now = GetWorld()->GetTimeSeconds();
	const FLinearColor RoundRed(0.75f, 0.06f, 0.04f, 0.95f);

	// Top-left: round number, zombies left, kills.
	if (GS->Round > 0)
	{
		// The round number painted over a dried blood smear; it throbs when a new round starts.
		const float RoundAge = Now - GS->RoundStartTime;
		const float Throb = GS->Phase == EZSRoundPhase::Active && RoundAge < 2.f ? 1.f + 0.15f * FMath::Sin(RoundAge * 12.f) * (1.f - RoundAge / 2.f) : 1.f;
		DrawSmear(10.f * Scale, 0.f, 360.f * Scale, 210.f * Scale, 0.85f);
		const FString RoundText = FString::Printf(TEXT("%d"), GS->Round);
		DrawText(TEXT("ROUND"), HudShadow, 46.f * Scale, 36.f * Scale, Small, 1.1f * Scale);
		DrawText(TEXT("ROUND"), LabelColor, 44.f * Scale, 34.f * Scale, Small, 1.1f * Scale);
		const float RS = 7.5f * Scale * Throb;
		DrawText(RoundText, FLinearColor(0.f, 0.f, 0.f, 0.75f), 48.f * Scale, 52.f * Scale, Big, RS);
		DrawText(RoundText, FLinearColor(0.95f, 0.9f, 0.82f, 0.97f), 44.f * Scale, 48.f * Scale, Big, RS);

		const float InfoY = 215.f * Scale;
		DrawPlate(30.f * Scale, InfoY - 6.f * Scale, 260.f * Scale, (GS->Phase == EZSRoundPhase::Active ? 64.f : 38.f) * Scale, PlateTint * FLinearColor(1, 1, 1, 0.8f));
		if (GS->Phase == EZSRoundPhase::Active)
		{
			DrawText(TEXT("INFECTED"), LabelColor, 46.f * Scale, InfoY, Small, 1.05f * Scale);
			const FString Left = FString::Printf(TEXT("%d"), GS->ZombiesRemaining);
			float LW, LH;
			GetTextSize(Left, LW, LH, Small, 1.35f * Scale);
			DrawText(Left, FLinearColor(0.95f, 0.3f, 0.2f, 1.f), 270.f * Scale - LW, InfoY - 3.f * Scale, Small, 1.35f * Scale);
		}
		const float KY = InfoY + (GS->Phase == EZSRoundPhase::Active ? 28.f : 0.f) * Scale;
		DrawText(TEXT("KILLS"), LabelColor * FLinearColor(1, 1, 1, 0.8f), 46.f * Scale, KY, Small, 1.05f * Scale);
		const FString KillsText = FString::Printf(TEXT("%d"), GS->Kills);
		float KW, KH;
		GetTextSize(KillsText, KW, KH, Small, 1.15f * Scale);
		DrawText(KillsText, HudWhite, 270.f * Scale - KW, KY - 1.f * Scale, Small, 1.15f * Scale);
	}

	const float CY = Canvas->ClipY * 0.26f;
	switch (GS->Phase)
	{
	case EZSRoundPhase::PreGame:
		DrawCentered(TEXT("GET READY"), CY, Big, 2.2f * Scale, HudWhite);
		DrawCentered(FString::Printf(TEXT("%d"), FMath::CeilToInt(GS->PhaseTimeLeft)), CY + 60.f * Scale, Big, 1.6f * Scale, HeadGold);
		break;

	case EZSRoundPhase::Active:
	{
		const float Age = Now - GS->RoundStartTime;
		if (Age < 3.5f)
		{
			FLinearColor C = RoundRed;
			C.A = FMath::Clamp(3.5f - Age, 0.f, 1.f);
			DrawCentered(FString::Printf(TEXT("ROUND %d"), GS->Round), CY, Big, 3.f * Scale, C);
		}
		break;
	}

	case EZSRoundPhase::Break:
		DrawCentered(FString::Printf(TEXT("ROUND %d SURVIVED"), GS->Round), CY, Big, 2.2f * Scale, HudWhite);
		DrawCentered(FString::Printf(TEXT("NEXT ROUND IN %d"), FMath::CeilToInt(GS->PhaseTimeLeft)), CY + 60.f * Scale, Small, 1.5f * Scale, HeadGold);
		break;

	case EZSRoundPhase::GameOver:
		DrawResults(GS, Scale);
		break;
	}

	if (GS->Phase != EZSRoundPhase::GameOver)
	{
		DrawRunEconomy(GS, Scale);
		DrawTask(GS, Scale);
		DrawMissionTracker(Scale);
		DrawPowerUps(GS, Scale);
		DrawBoss(GS, Scale);
	}
	DrawPauseMenu(Scale);
}

void AZSHUD::DrawBunker(AZSBunkerGameMode* GM, float Scale)
{
	const UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this);
	const AZSPlayerController* PC = Cast<AZSPlayerController>(GetOwningPlayerController());
	if (!Profile || (PC && PC->IsMenuOpen()))
	{
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	const float X = 70.f * Scale;
	float Y = 50.f * Scale;
	DrawPlate(40.f * Scale, 30.f * Scale, 380.f * Scale, 220.f * Scale, PlateTint);
	auto Line = [&](const FString& Text, float TextScale, const FLinearColor& Color)
	{
		DrawText(Text, HudShadow, X + 2.f, Y + 2.f, Font, TextScale * Scale);
		DrawText(Text, Color, X, Y, Font, TextScale * Scale);
		Y += 30.f * TextScale * Scale;
	};
	Line(TEXT("THE BUNKER"), 1.8f, HeadGold);
	Line(FString::Printf(TEXT("LEVEL %d"), Profile->GetLevel()), 1.4f, HudWhite);
	if (const int32 Rank = Profile->GetPrestige())
	{
		Line(FString::Printf(TEXT("PRESTIGE %d  %s"), Rank, *UZSProfileSubsystem::PrestigeTier(Rank).ToString().ToUpper()), 1.1f, UZSProfileSubsystem::PrestigeColor(Rank));
	}
	else if (Profile->CanPrestige())
	{
		Line(TEXT("PRESTIGE READY - MISSION BOARD"), 1.1f, HeadGold);
	}

	// XP bar.
	const float BarW = 320.f * Scale, BarH = 8.f * Scale;
	const float Pct = FMath::Clamp(static_cast<float>(Profile->GetLevelXP()) / FMath::Max(1, Profile->GetXPToNextLevel()), 0.f, 1.f);
	FCanvasTileItem Back(FVector2D(X, Y), FVector2D(BarW, BarH), FLinearColor(0.f, 0.f, 0.f, 0.5f));
	Back.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Back);
	FCanvasTileItem Fill(FVector2D(X, Y), FVector2D(BarW * Pct, BarH), FLinearColor(0.95f, 0.75f, 0.25f, 0.95f));
	Fill.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Fill);
	Y += 16.f * Scale;
	Line(FString::Printf(TEXT("%d / %d XP"), Profile->GetLevelXP(), Profile->GetXPToNextLevel()), 1.f, FLinearColor(0.8f, 0.8f, 0.8f, 0.9f));
	Line(FString::Printf(TEXT("$%lld"), Profile->GetCash()), 1.8f, FLinearColor(0.55f, 0.95f, 0.45f, 0.95f));

	FString Loadout;
	for (const FText& N : Profile->GetLoadoutNames())
	{
		Loadout += (Loadout.IsEmpty() ? TEXT("") : TEXT(" / ")) + N.ToString();
	}
	Line(FString::Printf(TEXT("LOADOUT %d:  %s"), Profile->GetActiveLoadoutIndex() + 1, Loadout.IsEmpty() ? TEXT("(empty)") : *Loadout), 1.1f, HudWhite);

	// Small dot so stations are easy to aim at.
	FCanvasTileItem Dot(FVector2D(Canvas->ClipX * 0.5f - 2.f * Scale, Canvas->ClipY * 0.5f - 2.f * Scale), FVector2D(4.f * Scale, 4.f * Scale), HudWhite);
	Dot.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Dot);

	DrawCentered(TEXT("Walk up to a station and press Interact.  Deploy from the MAP TABLE."), Canvas->ClipY - 70.f * Scale, Font, 1.1f * Scale, FLinearColor(0.85f, 0.85f, 0.85f, 0.8f));

	if (GM->NoticeTime > 0.f)
	{
		GM->NoticeTime -= GetWorld()->GetDeltaSeconds();
		DrawCentered(GM->Notice.ToString(), Canvas->ClipY * 0.3f, Font, 1.3f * Scale, HeadGold);
	}
}

void AZSHUD::DrawTask(const AZSGameState_Survival* GS, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const float Now = GetWorld()->GetTimeSeconds();
	const float X = 72.f * Scale;
	float Y = 215.f * Scale;
	auto Text = [&](const FString& S, float TextScale, const FLinearColor& C)
	{
		DrawText(S, HudShadow, X + 2.f, Y + 2.f, Font, TextScale * Scale);
		DrawText(S, C, X, Y, Font, TextScale * Scale);
		Y += 28.f * TextScale * Scale;
	};

	if (GS->bTaskActive)
	{
		const FLinearColor Orange(1.f, 0.6f, 0.15f, 1.f);
		Text(FString::Printf(TEXT("TASK: %s"), *GS->TaskTitle.ToString()), 1.35f, Orange);
		Text(GS->TaskStatus.ToString(), 1.05f, HudWhite);
		// Time bar (and progress, where it applies).
		const float BarW = 300.f * Scale, BarH = 6.f * Scale;
		FCanvasTileItem Back(FVector2D(X, Y), FVector2D(BarW, BarH), FLinearColor(0.f, 0.f, 0.f, 0.5f));
		Back.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Back);
		FCanvasTileItem Fill(FVector2D(X, Y), FVector2D(BarW * FMath::Clamp(GS->TaskProgress, 0.f, 1.f), BarH), FLinearColor(0.4f, 0.95f, 0.4f, 0.95f));
		Fill.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Fill);
		Y += 14.f * Scale;
		const bool bHurry = GS->TaskTimeLeft < 10.f;
		Text(FString::Printf(TEXT("%d s"), FMath::CeilToInt(GS->TaskTimeLeft)), 1.2f, bHurry ? KillRed : HudWhite);
		if (Now - GS->TaskStartTime < 3.f)
		{
			DrawCentered(FString::Printf(TEXT("NEW TASK: %s"), *GS->TaskTitle.ToString()), Canvas->ClipY * 0.34f, Font, 1.8f * Scale, Orange);
		}

		// Beacon indicator: on screen above the marker, or clamped to the screen edge.
		const APlayerController* PC = GetOwningPlayerController();
		if (GS->bTaskHasMarker && PC && PC->GetPawn())
		{
			FVector2D P;
			const FVector World = GS->TaskMarker + FVector(0.f, 0.f, 150.f);
			const bool bOnScreen = PC->ProjectWorldLocationToScreen(World, P, false);
			const FVector ToMarker = World - PC->PlayerCameraManager->GetCameraLocation();
			const bool bBehind = FVector::DotProduct(ToMarker, PC->PlayerCameraManager->GetCameraRotation().Vector()) < 0.f;
			const float M = 40.f * Scale;
			if (!bOnScreen || bBehind)
			{
				// Point toward it along the bottom of the screen.
				const FVector Local = PC->PlayerCameraManager->GetCameraRotation().UnrotateVector(ToMarker);
				P.X = Local.Y >= 0.f ? Canvas->ClipX - M : M;
				P.Y = Canvas->ClipY * 0.5f;
			}
			P.X = FMath::Clamp(P.X, M, Canvas->ClipX - M);
			P.Y = FMath::Clamp(P.Y, M, Canvas->ClipY - M);
			const float Dist = FVector::Dist(PC->GetPawn()->GetActorLocation(), GS->TaskMarker) / 100.f;
			const FLinearColor C = GS->TaskTitle.ToString().Contains(TEXT("BEACON")) ? FLinearColor(0.3f, 0.85f, 1.f, 1.f) : Orange;
			const float D = 10.f * Scale;
			DrawLine(P.X, P.Y - D, P.X + D, P.Y, C, 3.f);
			DrawLine(P.X + D, P.Y, P.X, P.Y + D, C, 3.f);
			DrawLine(P.X, P.Y + D, P.X - D, P.Y, C, 3.f);
			DrawLine(P.X - D, P.Y, P.X, P.Y - D, C, 3.f);
			const FString DistText = FString::Printf(TEXT("%.0f m"), Dist);
			float TW, TH;
			GetTextSize(DistText, TW, TH, Font, Scale);
			DrawText(DistText, C, P.X - TW * 0.5f, P.Y + D + 4.f, Font, Scale);
		}
	}
	else if (Now - GS->TaskResultTime < 3.f && !GS->TaskResult.IsEmpty())
	{
		const bool bOk = GS->TaskResult.ToString().Contains(TEXT("COMPLETE"));
		DrawCentered(GS->TaskResult.ToString(), Canvas->ClipY * 0.34f, Font, 2.f * Scale, bOk ? HeadGold : KillRed);
	}
}

void AZSHUD::AddScreenBlood(const APawn* Player, float Amount)
{
	const APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	AZSHUD* HUD = PC ? Cast<AZSHUD>(PC->GetHUD()) : nullptr;
	if (!HUD)
	{
		return;
	}
	const int32 Count = FMath::Clamp(FMath::RoundToInt(Amount * 7.f), 1, 10);
	for (int32 i = 0; i < Count; ++i)
	{
		FBloodBlob& B = HUD->BloodBlobs.AddDefaulted_GetRef();
		// Mostly towards the edges, some across the middle for big splatters.
		const float Edge = FMath::FRand() < 0.75f ? 1.f : 0.5f;
		B.Pos = FVector2D(FMath::FRandRange(0.05f, 0.95f), FMath::FRandRange(0.05f, 0.95f));
		B.Pos = FVector2D(0.5f, 0.5f) + (B.Pos - FVector2D(0.5f, 0.5f)).GetSafeNormal() * FMath::FRandRange(0.2f, 0.48f) * Edge;
		B.Radius = FMath::FRandRange(30.f, 90.f) * (0.6f + Amount);
		B.MaxLife = B.Life = FMath::FRandRange(1.8f, 3.2f);
		for (int32 s = 0; s < 16; ++s) { B.Shape.Add(FMath::FRandRange(0.65f, 1.25f)); }
	}
	while (HUD->BloodBlobs.Num() > 30) { HUD->BloodBlobs.RemoveAt(0); }
}

void AZSHUD::DrawScreenBlood(float DeltaTime)
{
	const float Scale = Canvas->ClipY / 1080.f;
	for (int32 i = BloodBlobs.Num() - 1; i >= 0; --i)
	{
		FBloodBlob& B = BloodBlobs[i];
		B.Life -= DeltaTime;
		if (B.Life <= 0.f)
		{
			BloodBlobs.RemoveAtSwap(i);
			continue;
		}
		const float A = FMath::Clamp(B.Life / (B.MaxLife * 0.6f), 0.f, 1.f) * 0.8f;
		const FVector2D C(B.Pos.X * Canvas->ClipX, B.Pos.Y * Canvas->ClipY + (B.MaxLife - B.Life) * 6.f * Scale);   // slowly runs down
		const float R = B.Radius * Scale;
		const FLinearColor Col(0.35f, 0.f, 0.f, A);
		const int32 N = B.Shape.Num();
		for (int32 s = 0; s < N; ++s)
		{
			const float A0 = 2.f * PI * s / N, A1 = 2.f * PI * (s + 1) / N;
			const FVector2D P0 = C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R * B.Shape[s];
			const FVector2D P1 = C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R * B.Shape[(s + 1) % N];
			FCanvasTriangleItem Tri(C, P0, P1, GWhiteTexture);
			Tri.SetColor(Col);
			Tri.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Tri);
		}
	}
}

void AZSHUD::DrawBoss(const AZSGameState_Survival* GS, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	UFont* Big = GEngine->GetLargeFont();
	const float Now = GetWorld()->GetTimeSeconds();
	if (GS->bBossActive)
	{
		const float W = 760.f * Scale, H = 16.f * Scale;
		const float X = (Canvas->ClipX - W) * 0.5f, Y = 70.f * Scale;
		DrawCentered(GS->BossName.ToString(), Y - 38.f * Scale, Font, 1.5f * Scale, FLinearColor(0.95f, 0.3f, 0.2f, 1.f));
		FCanvasTileItem Back(FVector2D(X - 3.f, Y - 3.f), FVector2D(W + 6.f, H + 6.f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
		Back.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Back);
		const FLinearColor Fill = GS->bBossExposed ? FLinearColor(1.f, 0.85f, 0.2f, 1.f) : FLinearColor(0.75f, 0.06f, 0.04f, 1.f);
		FCanvasTileItem Bar(FVector2D(X, Y), FVector2D(W * FMath::Clamp(GS->BossHealthPct, 0.f, 1.f), H), Fill);
		Canvas->DrawItem(Bar);
		// Phase marks at 66 % and 33 %.
		for (const float Mark : { 0.66f, 0.33f })
		{
			FCanvasTileItem Tick(FVector2D(X + W * Mark - 1.f, Y - 4.f), FVector2D(2.f * Scale, H + 8.f), FLinearColor(1.f, 1.f, 1.f, 0.7f));
			Canvas->DrawItem(Tick);
		}
		DrawCentered(FString::Printf(TEXT("PHASE %d%s"), GS->BossPhase, GS->bBossExposed ? TEXT("   -   WEAK POINTS EXPOSED") : TEXT("")),
			Y + H + 6.f * Scale, Font, 1.f * Scale, GS->bBossExposed ? HeadGold : HudWhite);
	}
	if (Now - GS->BossCalloutTime < 2.5f && !GS->BossCallout.IsEmpty())
	{
		FLinearColor C(1.f, 0.4f, 0.25f, FMath::Clamp(2.5f - (Now - GS->BossCalloutTime), 0.f, 1.f));
		DrawCentered(GS->BossCallout.ToString(), Canvas->ClipY * 0.2f, Font, 1.6f * Scale, C);
	}
	if (Now - GS->BossDeathTime < 1.5f)   // world seconds (slow motion makes this ~3 s)
	{
		DrawCentered(TEXT("THE ABOMINATION IS DEAD"), Canvas->ClipY * 0.3f, Big, 3.f * Scale, HeadGold);
		DrawCentered(TEXT("YOU SURVIVED"), Canvas->ClipY * 0.3f + 80.f * Scale, Big, 1.8f * Scale, HudWhite);
	}
}

void AZSHUD::DrawPowerUps(const AZSGameState_Survival* GS, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const float Now = GetWorld()->GetTimeSeconds();
	TArray<EZSPowerUp> Active;
	for (int32 i = 1; i < (int32)EZSPowerUp::MAX; ++i)
	{
		if (GS->PowerUpTimeLeft((EZSPowerUp)i) > 0.f) { Active.Add((EZSPowerUp)i); }
	}
	const float W = 190.f * Scale;
	float X = Canvas->ClipX * 0.5f - Active.Num() * W * 0.5f;
	const float Y = Canvas->ClipY - 150.f * Scale;
	for (const EZSPowerUp P : Active)
	{
		const float Left = GS->PowerUpTimeLeft(P);
		FLinearColor C = AZSGameState_Survival::PowerUpColor(P);
		if (Left < 4.f && FMath::Fmod(Now * 4.f, 1.f) < 0.4f) { C.A = 0.35f; }
		const FString Name = AZSGameState_Survival::PowerUpName(P).ToString();
		float TW, TH;
		GetTextSize(Name, TW, TH, Font, 1.2f * Scale);
		DrawText(Name, HudShadow, X + (W - TW) * 0.5f + 2.f, Y + 2.f, Font, 1.2f * Scale);
		DrawText(Name, C, X + (W - TW) * 0.5f, Y, Font, 1.2f * Scale);
		const float Dur = FMath::Max(1.f, GS->PowerUpDuration[(int32)P]);
		FCanvasTileItem Bar(FVector2D(X + 20.f * Scale, Y + 30.f * Scale), FVector2D((W - 40.f * Scale) * Left / Dur, 5.f * Scale), C);
		Bar.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Bar);
		X += W;
	}
	if (Now - GS->PowerUpFlashTime < 2.f)
	{
		FLinearColor C = HeadGold;
		C.A = FMath::Clamp(2.f - (Now - GS->PowerUpFlashTime), 0.f, 1.f);
		DrawCentered(GS->PowerUpFlash.ToString(), Canvas->ClipY * 0.4f, GEngine->GetLargeFont(), 2.4f * Scale, C);
	}
}

void AZSHUD::DrawMissionTracker(float Scale)
{
	const UZSMissionSubsystem* Missions = UZSMissionSubsystem::Get(this);
	if (!Missions || !Missions->IsRunActive())
	{
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	const float X = 72.f * Scale;
	float Y = Canvas->ClipY * 0.52f;
	int32 Shown = 0;
	for (const FZSMissionStatus& St : Missions->GetActive())
	{
		if (St.bComplete || Shown >= 3)
		{
			continue;
		}
		const bool bDoneThisRun = St.Total() >= St.Mission.Target;
		const FString Line = FString::Printf(TEXT("%s  %d/%d"), *St.Mission.Title.ToString(), St.Total(), St.Mission.Target);
		const FLinearColor C = bDoneThisRun ? HeadGold : FLinearColor(0.85f, 0.85f, 0.85f, 0.75f);
		DrawText(Line, HudShadow, X + 1.f, Y + 1.f, Font, 0.95f * Scale);
		DrawText(Line, C, X, Y, Font, 0.95f * Scale);
		Y += 24.f * Scale;
		++Shown;
	}
}

void AZSHUD::DrawRunEconomy(const AZSGameState_Survival* GS, float Scale)
{
	UFont* Small = GEngine->GetMediumFont();
	const float Now = GetWorld()->GetTimeSeconds();
	const float Right = Canvas->ClipX - 70.f * Scale;

	// Run earnings, top right - lost on a fail until the run is won.
	auto RightText = [&](const FString& Text, float Y, float TextScale, const FLinearColor& Color)
	{
		float TW, TH;
		GetTextSize(Text, TW, TH, Small, TextScale);
		DrawText(Text, HudShadow, Right - TW + 2.f, Y + 2.f, Small, TextScale);
		DrawText(Text, Color, Right - TW, Y, Small, TextScale);
	};
	DrawPlate(Right - 230.f * Scale, 36.f * Scale, 250.f * Scale, 112.f * Scale, PlateTint);
	RightText(FString::Printf(TEXT("$%d"), GS->RunCash), 50.f * Scale, 2.f * Scale, FLinearColor(0.55f, 0.95f, 0.45f, 0.95f));
	RightText(FString::Printf(TEXT("%d XP"), GS->RunXP), 92.f * Scale, 1.4f * Scale, HudWhite);
	RightText(TEXT("AT RISK"), 122.f * Scale, 1.f * Scale, FLinearColor(0.9f, 0.4f, 0.3f, 0.8f));

	// Reward popups, right of the crosshair, drifting up.
	float Y = Canvas->ClipY * 0.5f - 20.f * Scale;
	for (int32 i = GS->Popups.Num() - 1; i >= 0; --i)
	{
		const FZSRewardPopup& P = GS->Popups[i];
		const float Age = Now - P.Time;
		const float Life = P.bBig ? 3.f : 1.4f;
		if (Age > Life)
		{
			continue;
		}
		FLinearColor C = P.bBig ? HeadGold : FLinearColor(0.85f, 0.95f, 0.8f, 1.f);
		C.A = FMath::Clamp((Life - Age) / 0.4f, 0.f, 1.f);
		const float S = (P.bBig ? 1.5f : 1.1f) * Scale;
		const float X = Canvas->ClipX * 0.5f + 60.f * Scale;
		const float PY = Y - Age * 18.f * Scale;
		DrawText(P.Text.ToString(), HudShadow * FLinearColor(1, 1, 1, C.A), X + 2.f, PY + 2.f, Small, S);
		DrawText(P.Text.ToString(), C, X, PY, Small, S);
		Y -= 26.f * Scale;
	}

	// Deploy: what a fail would cost, plus any one-off notice.
	if (GS->Phase == EZSRoundPhase::PreGame)
	{
		FString Risk;
		for (const FText& T : GS->AtRisk)
		{
			Risk += (Risk.IsEmpty() ? TEXT("") : TEXT(",  ")) + T.ToString();
		}
		if (!Risk.IsEmpty())
		{
			DrawCentered(FString::Printf(TEXT("AT RISK:  %s"), *Risk), Canvas->ClipY * 0.26f + 120.f * Scale, Small, 1.3f * Scale, FLinearColor(0.95f, 0.45f, 0.35f, 1.f));
			DrawCentered(TEXT("Die or abandon and you lose this gear and everything you earn this run."), Canvas->ClipY * 0.26f + 152.f * Scale, Small, 1.05f * Scale, FLinearColor(0.8f, 0.8f, 0.8f, 0.9f));
		}
		if (!GS->Notice.IsEmpty())
		{
			DrawCentered(GS->Notice.ToString(), Canvas->ClipY * 0.26f + 190.f * Scale, Small, 1.1f * Scale, HeadGold);
		}
	}
}

void AZSHUD::DrawResults(const AZSGameState_Survival* GS, float Scale)
{
	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetMediumFont();
	const FZSRunResult& R = GS->Result;
	const bool bWon = R.Outcome == EZSRunOutcome::Won;

	DrawRect(bWon ? FLinearColor(0.f, 0.05f, 0.02f, 0.78f) : FLinearColor(0.07f, 0.f, 0.f, 0.8f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

	const FString Title = bWon ? TEXT("RUN COMPLETE") : (R.Outcome == EZSRunOutcome::Abandoned ? TEXT("RUN ABANDONED") : TEXT("RUN FAILED"));
	float Y = Canvas->ClipY * 0.14f;
	DrawCentered(Title, Y, Big, 4.f * Scale, bWon ? FLinearColor(0.45f, 1.f, 0.5f, 1.f) : KillRed);
	Y += 110.f * Scale;

	const int32 Minutes = FMath::FloorToInt(R.RunTime / 60.f);
	const int32 Seconds = FMath::FloorToInt(R.RunTime) % 60;
	DrawCentered(FString::Printf(TEXT("Round %d      Kills %d      Headshots %d      Time %d:%02d"), R.RoundReached, R.Kills, R.Headshots, Minutes, Seconds),
		Y, Small, 1.5f * Scale, HudWhite);
	Y += 60.f * Scale;

	// Earnings: kept (green) or lost (red, "LOST").
	const FLinearColor Kept(0.5f, 1.f, 0.5f, 1.f);
	const FLinearColor Lost(1.f, 0.35f, 0.3f, 1.f);
	if (bWon)
	{
		DrawCentered(FString::Printf(TEXT("XP  %d  + %d win bonus  =  %d"), R.XPEarned, R.XPBonus, R.XPKept), Y, Small, 1.5f * Scale, Kept);
		Y += 36.f * Scale;
		DrawCentered(FString::Printf(TEXT("CASH  $%d  + $%d win bonus  =  $%d"), R.CashEarned, R.CashBonus, R.CashKept), Y, Small, 1.5f * Scale, Kept);
		Y += 36.f * Scale;
		DrawCentered(TEXT("Loadout kept"), Y, Small, 1.2f * Scale, Kept);
	}
	else
	{
		DrawCentered(FString::Printf(TEXT("XP  %d   LOST"), R.XPEarned), Y, Small, 1.5f * Scale, Lost);
		Y += 36.f * Scale;
		DrawCentered(FString::Printf(TEXT("CASH  $%d   LOST"), R.CashEarned), Y, Small, 1.5f * Scale, Lost);
		Y += 36.f * Scale;
		FString Items;
		for (const FText& T : R.LostItems)
		{
			Items += (Items.IsEmpty() ? TEXT("") : TEXT(",  ")) + T.ToString();
		}
		DrawCentered(Items.IsEmpty() ? FString(TEXT("No gear lost")) : FString::Printf(TEXT("GEAR LOST:  %s"), *Items), Y, Small, 1.2f * Scale, Lost);
	}
	Y += 60.f * Scale;

	// Level bar.
	const float BarW = 600.f * Scale, BarH = 14.f * Scale;
	const float BX = Canvas->ClipX * 0.5f - BarW * 0.5f;
	const float Frac = FMath::Clamp(static_cast<float>(R.LevelXP) / FMath::Max(R.LevelXPNeeded, 1), 0.f, 1.f);
	DrawCentered(FString::Printf(TEXT("LEVEL %d"), R.LevelAfter), Y, Small, 1.6f * Scale, HudWhite);
	Y += 40.f * Scale;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), BX - 2.f, Y - 2.f, BarW + 4.f, BarH + 4.f);
	DrawRect(FLinearColor(0.95f, 0.8f, 0.25f, 0.95f), BX, Y, BarW * Frac, BarH);
	Y += BarH + 10.f * Scale;
	DrawCentered(FString::Printf(TEXT("%d / %d XP"), R.LevelXP, R.LevelXPNeeded), Y, Small, 1.1f * Scale, FLinearColor(0.8f, 0.8f, 0.8f, 1.f));
	Y += 34.f * Scale;
	if (R.LevelAfter > R.LevelBefore)
	{
		DrawCentered(FString::Printf(TEXT("LEVEL UP!  %d  ->  %d"), R.LevelBefore, R.LevelAfter), Y, Big, 1.6f * Scale, HeadGold);
		Y += 50.f * Scale;
	}
	DrawCentered(FString::Printf(TEXT("Cash in your profile: $%lld"), GS->ProfileCash), Y, Small, 1.2f * Scale, HudWhite);
	Y += 40.f * Scale;
	// Unlocks and missions (skip the plain "Level N reached" lines - the level-up line covers them).
	for (const FText& T : R.Unlocks)
	{
		const FString S = T.ToString();
		if (S.StartsWith(TEXT("Level "))) { continue; }
		const bool bLost = S.Contains(TEXT("lost"));
		DrawCentered(S, Y, Small, 1.05f * Scale, bLost ? FLinearColor(0.95f, 0.45f, 0.35f, 1.f) : FLinearColor(0.7f, 0.95f, 0.6f, 1.f));
		Y += 28.f * Scale;
	}

	DrawCentered(FString::Printf(TEXT("Press A / Enter to continue   (%d)"), FMath::CeilToInt(GS->RestartIn)), Canvas->ClipY * 0.88f, Small, 1.3f * Scale, HeadGold);
}

void AZSHUD::DrawPauseMenu(float Scale)
{
	const AZSPlayerController* PC = Cast<AZSPlayerController>(GetOwningPlayerController());
	const FZSMenuPage* Page = PC ? PC->GetTopMenu() : nullptr;
	if (!Page)
	{
		return;
	}
	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetMediumFont();
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.68f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

	// Left: title + list. Right: detail of the highlighted item.
	const float ListX = Canvas->ClipX * 0.12f;
	const float ListW = Canvas->ClipX * 0.42f;
	float Y = Canvas->ClipY * 0.14f;
	DrawText(Page->Title.ToString(), HudShadow, ListX + 2.f, Y + 2.f, Big, 2.6f * Scale);
	DrawText(Page->Title.ToString(), HudWhite, ListX, Y, Big, 2.6f * Scale);
	Y += 80.f * Scale;
	if (!Page->Subtitle.IsEmpty())
	{
		DrawText(Page->Subtitle.ToString(), FLinearColor(0.75f, 0.75f, 0.75f, 1.f), ListX, Y, Small, 1.15f * Scale);
		Y += 40.f * Scale;
	}

	// Scroll so the selection stays visible.
	const int32 MaxRows = 11;
	const int32 First = FMath::Clamp(Page->Index - MaxRows / 2, 0, FMath::Max(0, Page->Items.Num() - MaxRows));
	for (int32 i = First; i < FMath::Min(Page->Items.Num(), First + MaxRows); ++i)
	{
		const FZSMenuItem& Item = Page->Items[i];
		const bool bSel = i == Page->Index;
		const float RowH = 44.f * Scale;
		if (bSel)
		{
			DrawRect(FLinearColor(1.f, 0.8f, 0.2f, 0.18f), ListX - 12.f * Scale, Y - 4.f * Scale, ListW + 24.f * Scale, RowH - 4.f * Scale);
		}
		FLinearColor C = !Item.bEnabled ? FLinearColor(0.45f, 0.45f, 0.45f, 1.f) : (bSel ? HeadGold : HudWhite);
		DrawText(Item.Label.ToString(), C, ListX, Y, Small, 1.4f * Scale);
		if (!Item.Value.IsEmpty())
		{
			float VW, VH;
			GetTextSize(Item.Value.ToString(), VW, VH, Small, 1.25f * Scale);
			DrawText(Item.Value.ToString(), Item.bEnabled ? FLinearColor(0.75f, 0.9f, 0.75f, 1.f) : C, ListX + ListW - VW, Y + 2.f * Scale, Small, 1.25f * Scale);
		}
		Y += RowH;
	}
	if (!Page->bHideDetail && Page->Items.IsValidIndex(Page->Index) && !Page->Items[Page->Index].Detail.IsEmpty())
	{
		const float DX = Canvas->ClipX * 0.6f;
		const float DY = Canvas->ClipY * 0.3f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), DX - 20.f * Scale, DY - 20.f * Scale, Canvas->ClipX * 0.3f + 40.f * Scale, Canvas->ClipY * 0.4f);
		// Simple word wrap.
		TArray<FString> Lines;
		Page->Items[Page->Index].Detail.ToString().ParseIntoArrayLines(Lines);
		float LY = DY;
		const float MaxW = Canvas->ClipX * 0.3f;
		for (const FString& Para : Lines)
		{
			TArray<FString> Words;
			Para.ParseIntoArrayWS(Words);
			FString Line;
			for (const FString& W : Words)
			{
				const FString Test = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
				float TW, TH;
				GetTextSize(Test, TW, TH, Small, 1.15f * Scale);
				if (TW > MaxW && !Line.IsEmpty())
				{
					DrawText(Line, HudWhite, DX, LY, Small, 1.15f * Scale);
					LY += 30.f * Scale;
					Line = W;
				}
				else
				{
					Line = Test;
				}
			}
			DrawText(Line, HudWhite, DX, LY, Small, 1.15f * Scale);
			LY += 38.f * Scale;
		}
	}
	DrawCentered(TEXT("D-pad / arrows: choose      A / Enter: select      B / Backspace: back"), Canvas->ClipY * 0.92f, Small, 1.05f * Scale, FLinearColor(0.7f, 0.7f, 0.7f, 1.f));
}
