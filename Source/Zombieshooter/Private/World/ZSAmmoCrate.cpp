#include "World/ZSAmmoCrate.h"
#include "Missions/ZSMissionSubsystem.h"
#include "Player/ZSPlayerCharacter.h"
#include "Weapons/ZSWeaponComponent.h"
#include "UI/ZSHUD.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

namespace
{
	const TCHAR* BoxPath(const TCHAR* Name)
	{
		static TMap<FString, FString> Cache;
		FString& P = Cache.FindOrAdd(Name);
		if (P.IsEmpty())
		{
			P = FString::Printf(TEXT("/Game/ZS_Imported/Props/AmmoBoxes/%s.%s"), Name, Name);
		}
		return *P;
	}
}

AZSAmmoCrate::AZSAmmoCrate()
{
	PrimaryActorTick.bCanEverTick = false;

	auto MakeMesh = [this](const TCHAR* Name, USceneComponent* Parent)
	{
		UStaticMeshComponent* M = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		if (Parent)
		{
			M->SetupAttachment(Parent);
		}
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return M;
	};

	Base = MakeMesh(TEXT("Base"), nullptr);
	RootComponent = Base;
	BoxA = MakeMesh(TEXT("BoxA"), Base);
	BoxB = MakeMesh(TEXT("BoxB"), Base);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetupAttachment(Base);
	Collision->SetUsingAbsoluteScale(true);
	Collision->SetBoxExtent(FVector(48.f, 52.f, 32.f));
	Collision->SetRelativeLocation(FVector(5.f, -4.f, 20.f));
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ZS_TRACE_INTERACT, ECR_Block);
	Collision->SetCanEverAffectNavigation(true);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Base);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(20.f);
	Light->SetAttenuationRadius(350.f);
	Light->SetLightColor(FLinearColor(1.f, 0.8f, 0.4f));
	Light->SetCastShadows(false);

}

void AZSAmmoCrate::BeginPlay()
{
	Super::BeginPlay();
	ApplyVisuals();
	if (!RefillSound)
	{
		RefillSound = LoadObject<USoundBase>(nullptr,
			TEXT("/Game/InfimaGames/TacticalFPSAnimations/Weapons/AssaultRifle/Audio/Handling/Magazine/A_TFA_AR_Mag_Insert_Cue.A_TFA_AR_Mag_Insert_Cue"),
			nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

void AZSAmmoCrate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyVisuals();
}

void AZSAmmoCrate::ApplyVisuals()
{

	UStaticMesh* Big = LoadObject<UStaticMesh>(nullptr, BoxPath(TEXT("SM_Ammobox_50")), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UStaticMesh* Small = LoadObject<UStaticMesh>(nullptr, BoxPath(TEXT("SM_Ammobox_556_NATO_Closed")), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UStaticMesh* Mid = LoadObject<UStaticMesh>(nullptr, BoxPath(TEXT("SM_Ammobox_762_NATO_Closed")), nullptr, LOAD_NoWarn | LOAD_Quiet);

	if (Big && Small && Mid)
	{
		// Open .50 cal can on the floor (scaled up a little so it reads at distance), two closed cans beside it.
		Base->SetStaticMesh(Big);
		Base->SetRelativeScale3D(FVector(1.6f));
		BoxA->SetStaticMesh(Mid);
		BoxA->SetRelativeLocationAndRotation(FVector(32.f, 0.f, 0.f), FRotator(0.f, 80.f, 0.f));
		BoxB->SetStaticMesh(Small);
		BoxB->SetRelativeLocationAndRotation(FVector(30.f, 4.f, 17.f), FRotator(0.f, 20.f, 0.f));
	}
	else
	{
		// Placeholder: olive crate.
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		Base->SetStaticMesh(Cube);
		Base->SetRelativeScale3D(FVector(0.9f, 0.6f, 0.5f));
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.15f, 0.05f));
			Base->SetMaterial(0, MID);
		}
	}
}

FText AZSAmmoCrate::GetInteractPrompt() const
{
	return NSLOCTEXT("ZS", "RefillAmmo", "REFILL AMMO");
}

void AZSAmmoCrate::Interact(AZSPlayerCharacter* Player)
{
	if (!Player)
	{
		return;
	}
	if (UZSWeaponComponent* Weapons = Player->GetWeaponComponent())
	{
		Weapons->RefillAllAmmo();
	}
	if (RefillSound)
	{
		UGameplayStatics::PlaySound2D(this, RefillSound);
	}
	AZSHUD::Toast(Player, NSLOCTEXT("ZS", "AmmoRefilled", "AMMO REFILLED"));
	if (UZSMissionSubsystem* Missions = UZSMissionSubsystem::Get(this)) { Missions->Record(EZSMissionStat::CratesUsed); }
}
