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

	// Health bar, bottom-left.
	const float W = 320.f * Scale, H = 12.f * Scale;
	const float X = 70.f * Scale, Y = Canvas->ClipY - 70.f * Scale - H;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X - 2.f, Y - 2.f, W + 4.f, H + 4.f);
	const FLinearColor BarColor = Frac < 0.35f ? FLinearColor(0.9f, 0.15f, 0.1f, 0.95f) : FLinearColor(0.92f, 0.92f, 0.92f, 0.9f);
	DrawRect(BarColor, X, Y, W * Frac, H);
	UFont* Small = GEngine->GetMediumFont();
	DrawText(FString::Printf(TEXT("%d"), FMath::CeilToInt(Health->GetHealth())), HudWhite, X, Y - 26.f * Scale, Small, 1.3f * Scale);

	// Armor plates: one segment per 50 points, above the health bar.
	if (Health->MaxArmor > 0.f)
	{
		const int32 Plates = FMath::Max(1, FMath::RoundToInt(Health->MaxArmor / 50.f));
		const float Gap = 6.f * Scale;
		const float SegW = (W * 0.6f - Gap * (Plates - 1)) / Plates;
		const float SegH = 7.f * Scale;
		const float SegY = Y - 40.f * Scale;
		const float SegX0 = X + 60.f * Scale;
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
		const FString RoundText = FString::Printf(TEXT("%d"), GS->Round);
		DrawText(RoundText, HudShadow, 73.f * Scale, 33.f * Scale, Big, 6.5f * Scale);
		DrawText(RoundText, RoundRed, 70.f * Scale, 30.f * Scale, Big, 6.5f * Scale);
		if (GS->Phase == EZSRoundPhase::Active)
		{
			DrawText(FString::Printf(TEXT("ZOMBIES  %d"), GS->ZombiesRemaining), HudWhite, 72.f * Scale, 140.f * Scale, Small, 1.3f * Scale);
		}
		DrawText(FString::Printf(TEXT("KILLS  %d"), GS->Kills), FLinearColor(0.8f, 0.8f, 0.8f, 0.8f), 72.f * Scale, 168.f * Scale, Small, 1.15f * Scale);
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
	auto Line = [&](const FString& Text, float TextScale, const FLinearColor& Color)
	{
		DrawText(Text, HudShadow, X + 2.f, Y + 2.f, Font, TextScale * Scale);
		DrawText(Text, Color, X, Y, Font, TextScale * Scale);
		Y += 30.f * TextScale * Scale;
	};
	Line(TEXT("THE BUNKER"), 1.8f, HeadGold);
	Line(FString::Printf(TEXT("LEVEL %d"), Profile->GetLevel()), 1.4f, HudWhite);

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
	if (Page->Items.IsValidIndex(Page->Index) && !Page->Items[Page->Index].Detail.IsEmpty())
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
