#include "Core/ZSCharacterCatalog.h"
#include "Profile/ZSProfileTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace ZSCharacterCatalog
{
	const TArray<FBody>& Bodies()
	{
		static const TArray<FBody> List = {
			{ NSLOCTEXT("ZS", "Body1", "Gas-Mask Scout"),  TEXT("/Game/SurvivorsMegaPack/Survivor1/Mesh/SK_Survivor1.SK_Survivor1"),
				TEXT("/Game/SurvivorsMegaPack/Survivor1/Animations/Demo/ThirdPersonIdle.ThirdPersonIdle"), TEXT("/Game/SurvivorsMegaPack/Survivor1/Animations/Demo/ThirdPersonRun.ThirdPersonRun"), 0, 0, 1 },
			{ NSLOCTEXT("ZS", "Body2", "Street Runner"),   TEXT("/Game/SurvivorsMegaPack/Survivor2/Mesh/SK_Survivor2.SK_Survivor2"),
				TEXT("/Game/SurvivorsMegaPack/Survivor2/Animations/Demo/ThirdPersonIdle.ThirdPersonIdle"), TEXT("/Game/SurvivorsMegaPack/Survivor2/Animations/Demo/ThirdPersonRun.ThirdPersonRun"), 1, 0, 1 },
			{ NSLOCTEXT("ZS", "Body3", "Salvager"),        TEXT("/Game/SurvivorsMegaPack/Survivor3/Mesh/SK_Survivor3.SK_Survivor3"),
				TEXT("/Game/SurvivorsMegaPack/Survivor3/Animations/Demo/ThirdPersonIdle.ThirdPersonIdle"), TEXT("/Game/SurvivorsMegaPack/Survivor3/Animations/Demo/ThirdPersonRun.ThirdPersonRun"), 2, 3, 3 },
			{ NSLOCTEXT("ZS", "Body4", "Hazmat Worker"),   TEXT("/Game/SurvivorsMegaPack/Survivor4/Mesh/SK_Survivor4.SK_Survivor4"),
				TEXT("/Game/SurvivorsMegaPack/Survivor4/Animations/Demo/ThirdPersonIdle.ThirdPersonIdle"), TEXT("/Game/SurvivorsMegaPack/Survivor4/Animations/Demo/ThirdPersonRun.ThirdPersonRun"), 6, 2, 6 },
			{ NSLOCTEXT("ZS", "Body5", "Night Watch"),     TEXT("/Game/SurvivorsMegaPack/Survivor5/Mesh/SK_Survivor5.SK_Survivor5"),
				TEXT("/Game/SurvivorsMegaPack/Survivor5/Animations/Demo/ThirdPersonIdle.ThirdPersonIdle"), TEXT("/Game/SurvivorsMegaPack/Survivor5/Animations/Demo/ThirdPersonRun.ThirdPersonRun"), 8, 1, 10 },
		};
		return List;
	}

	const TArray<FArms>& Arms()
	{
		static const TArray<FArms> List = []()
		{
			TArray<FArms> A;
			const TCHAR* Names[] = { TEXT("Tactical Gloves"), TEXT("Work Gloves"), TEXT("Fingerless"), TEXT("Leather"), TEXT("Combat"),
				TEXT("Padded"), TEXT("Rubber"), TEXT("Knuckle Guard"), TEXT("Winter"), TEXT("Mechanic"), TEXT("Rider") };
			for (int32 i = 1; i <= 11; ++i)
			{
				A.Add({ FText::FromString(Names[i - 1]), nullptr, i <= 3 ? 1 : (i <= 6 ? 2 : (i <= 9 ? 5 : 8)) });
			}
			return A;
		}();
		return List;
	}

	static FString ArmsPath(int32 Index)
	{
		const int32 N = FMath::Clamp(Index, 0, 10) + 1;
		return FString::Printf(TEXT("/Game/%s/Modules/SK_Arms_%02d.SK_Arms_%02d"), N <= 5 ? TEXT("AAA_FPS_Gloves") : TEXT("AAA_FPS_Gloves_Vol2"), N, N);
	}

	const TArray<FTint>& Tints()
	{
		static const TArray<FTint> List = {
			{ NSLOCTEXT("ZS", "TintNone", "Factory"),    FLinearColor(1.f, 1.f, 1.f, 0.f) },
			{ NSLOCTEXT("ZS", "TintBlack", "Blackout"),  FLinearColor(0.03f, 0.03f, 0.035f, 1.f) },
			{ NSLOCTEXT("ZS", "TintOlive", "Olive"),     FLinearColor(0.12f, 0.14f, 0.06f, 1.f) },
			{ NSLOCTEXT("ZS", "TintTan", "Desert Tan"),  FLinearColor(0.42f, 0.32f, 0.2f, 1.f) },
			{ NSLOCTEXT("ZS", "TintNavy", "Navy"),       FLinearColor(0.04f, 0.06f, 0.15f, 1.f) },
			{ NSLOCTEXT("ZS", "TintRed", "Blood Red"),   FLinearColor(0.35f, 0.03f, 0.03f, 1.f) },
			{ NSLOCTEXT("ZS", "TintArctic", "Arctic"),   FLinearColor(0.7f, 0.72f, 0.75f, 1.f) },
			{ NSLOCTEXT("ZS", "TintGrey", "Urban Grey"), FLinearColor(0.2f, 0.2f, 0.21f, 1.f) },
		};
		return List;
	}

	int32 ResolveArms(const FZSCharacterLook& Look)
	{
		if (Look.bMatchArms && Bodies().IsValidIndex(Look.Body))
		{
			return Bodies()[Look.Body].MatchingArms;
		}
		return FMath::Clamp(Look.Arms, 0, Arms().Num() - 1);
	}

	int32 ResolveTint(const FZSCharacterLook& Look)
	{
		if (Look.bMatchArms && Bodies().IsValidIndex(Look.Body))
		{
			return Bodies()[Look.Body].MatchingTint;
		}
		return FMath::Clamp(Look.Tint, 0, Tints().Num() - 1);
	}

	USkeletalMesh* LoadBody(int32 Index)
	{
		return Bodies().IsValidIndex(Index) ? LoadObject<USkeletalMesh>(nullptr, Bodies()[Index].Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	}

	USkeletalMesh* LoadArms(int32 Index)
	{
		return LoadObject<USkeletalMesh>(nullptr, *ArmsPath(Index), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	UAnimSequence* LoadIdle(int32 Body)
	{
		return Bodies().IsValidIndex(Body) ? LoadObject<UAnimSequence>(nullptr, Bodies()[Body].Idle, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	}

	UAnimSequence* LoadRun(int32 Body)
	{
		return Bodies().IsValidIndex(Body) ? LoadObject<UAnimSequence>(nullptr, Bodies()[Body].Run, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	}

	void ApplyTint(USkinnedMeshComponent* Comp, int32 Tint)
	{
		if (!Comp || !Comp->GetSkinnedAsset())
		{
			return;
		}
		const FTint& T = Tints()[FMath::Clamp(Tint, 0, Tints().Num() - 1)];
		Comp->EmptyOverrideMaterials();
		if (T.Color.A <= 0.f)
		{
			return;
		}
		for (int32 i = 0; i < Comp->GetNumMaterials(); ++i)
		{
			if (UMaterialInstanceDynamic* MID = Comp->CreateDynamicMaterialInstance(i))
			{
				FLinearColor C = T.Color;
				C.A = 1.f;
				MID->SetVectorParameterValue(TEXT("Tint A"), C);
			}
		}
	}
}
