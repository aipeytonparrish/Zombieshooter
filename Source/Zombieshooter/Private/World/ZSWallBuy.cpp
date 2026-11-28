#include "World/ZSWallBuy.h"
#include "Player/ZSPlayerCharacter.h"
#include "Rounds/ZSGameState_Survival.h"
#include "UI/ZSHUD.h"
#include "Weapons/ZSWeaponBase.h"
#include "Weapons/ZSWeaponCatalog.h"
#include "Weapons/ZSWeaponComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Weapons/ZSWeaponTypes.h"
#include "Zombieshooter.h"

namespace ZSWeaponDisplay
{
	/** The gun's look from its class defaults: the animated rig's weapon mesh, or the static parts (no attachments). */
	void Build(AActor* Owner, USceneComponent* Parent, FName ItemId, TArray<TObjectPtr<USceneComponent>>& Out, const FTransform& Offset)
	{
		for (USceneComponent* C : Out) { if (C) { C->DestroyComponent(); } }
		Out.Reset();
		const ZSWeaponCatalog::FEntry* E = ZSWeaponCatalog::Find(ItemId);
		const AZSWeaponBase* CDO = E && E->Class ? E->Class->GetDefaultObject<AZSWeaponBase>() : nullptr;
		if (!CDO)
		{
			return;
		}
		// Side profile towards the viewer (+X): the barrel has to run along Y. Guns are modelled either way, so go by the
		// longer horizontal extent.
		if (USkeletalMesh* Mesh = CDO->AnimSet.WeaponMesh.LoadSynchronous())
		{
			USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(Owner, NAME_None, RF_Transient);
			C->SetupAttachment(Parent);
			C->SetSkeletalMeshAsset(Mesh);
			const FBoxSphereBounds B = Mesh->GetBounds();
			const FRotator Turn = B.BoxExtent.X > B.BoxExtent.Y ? FRotator(0.f, 90.f, 0.f) : FRotator::ZeroRotator;
			// Centre the gun on the mount.
			const FVector Centre = Turn.RotateVector(B.Origin);
			C->SetRelativeTransform(FTransform(Turn, -Centre) * Offset);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->RegisterComponent();
			Out.Add(C);
			return;
		}
		for (const FZSWeaponPart& Part : CDO->Stats.Parts)
		{
			if (!Part.RequiredAttachment.IsNone()) { continue; }
			UStaticMesh* Mesh = Part.Mesh.IsNull() ? nullptr : Part.Mesh.LoadSynchronous();
			if (!Mesh) { continue; }
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
			C->SetupAttachment(Parent);
			C->SetStaticMesh(Mesh);
			C->SetRelativeTransform(Part.Offset * FTransform(FRotator(0.f, 90.f, 0.f)) * Offset);   // parts are built X-forward
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->RegisterComponent();
			Out.Add(C);
		}
	}
}

AZSWallBuy::AZSWallBuy()
{
	PrimaryActorTick.bCanEverTick = false;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(20.f, 60.f, 30.f));
	Box->SetCollisionProfileName(TEXT("NoCollision"));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ZS_TRACE_INTERACT, ECR_Block);
	RootComponent = Box;

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Box);
	Light->SetRelativeLocation(FVector(45.f, 0.f, 45.f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(14.f);
	Light->SetAttenuationRadius(260.f);
	Light->SetLightColor(FLinearColor(1.f, 0.8f, 0.55f));
	Light->SetCastShadows(false);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Box);
	Label->SetRelativeLocation(FVector(4.f, 0.f, 34.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(6.f);
	Label->SetTextRenderColor(FColor(240, 220, 170));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AZSWallBuy::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildDisplay();
}

void AZSWallBuy::BeginPlay()
{
	Super::BeginPlay();
	BuildDisplay();
	BuySound = LoadObject<USoundBase>(nullptr, TEXT("/Game/InfimaGames/TacticalFPSAnimations/Common/Audio/Handling/Foley/A_TFA_AR_Handling_Cue.A_TFA_AR_Handling_Cue"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void AZSWallBuy::BuildDisplay()
{
	// The gun lies flat against the wall, barrel to the side.
	ZSWeaponDisplay::Build(this, Box, ItemId, DisplayParts, FTransform(FVector(8.f, 0.f, 0.f)));
	Label->SetText(FText::Format(NSLOCTEXT("ZS", "WallBuyLabel", "{0}  ${1}"), ZSWeaponCatalog::DisplayName(ItemId), FText::AsNumber(Cost)));
}

FText AZSWallBuy::GetInteractPrompt() const
{
	const AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const UZSWeaponComponent* Weapons = Char ? Char->GetWeaponComponent() : nullptr;
	if (Weapons && Weapons->HasWeapon(ItemId))
	{
		return FText::Format(NSLOCTEXT("ZS", "WallBuyAmmo", "BUY {0} AMMO  ${1}"), ZSWeaponCatalog::DisplayName(ItemId), FText::AsNumber(GetAmmoCost()));
	}
	return FText::Format(NSLOCTEXT("ZS", "WallBuyGun", "BUY {0}  ${1}"), ZSWeaponCatalog::DisplayName(ItemId), FText::AsNumber(Cost));
}

void AZSWallBuy::Interact(AZSPlayerCharacter* Player)
{
	UZSWeaponComponent* Weapons = Player ? Player->GetWeaponComponent() : nullptr;
	AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>();
	if (!Weapons || !GS)
	{
		return;
	}
	const bool bAmmo = Weapons->HasWeapon(ItemId);
	const int32 Price = bAmmo ? GetAmmoCost() : Cost;
	if (GS->RunCash < Price)
	{
		AZSHUD::Toast(Player, FText::Format(NSLOCTEXT("ZS", "WallBuyPoor", "NEED ${0} MORE"), FText::AsNumber(Price - GS->RunCash)));
		return;
	}
	GS->RunCash -= Price;
	if (bAmmo)
	{
		Weapons->RefillWeapon(ItemId);
		AZSHUD::Toast(Player, NSLOCTEXT("ZS", "WallBuyAmmoDone", "AMMO RESTOCKED"));
	}
	else
	{
		Weapons->GiveWeapon(ItemId, TArray<FName>());
	}
	if (BuySound)
	{
		UGameplayStatics::PlaySound2D(this, BuySound);
	}
}
