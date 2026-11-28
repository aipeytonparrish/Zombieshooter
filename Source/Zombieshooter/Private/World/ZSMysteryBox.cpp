#include "World/ZSMysteryBox.h"
#include "World/ZSWallBuy.h"
#include "Player/ZSPlayerCharacter.h"
#include "Rounds/ZSGameState_Survival.h"
#include "UI/ZSHUD.h"
#include "Weapons/ZSWeaponCatalog.h"
#include "Weapons/ZSWeaponComponent.h"
#include "Weapons/ZSWeaponTypes.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Zombieshooter.h"

namespace
{
	const float RollTime = 4.2f;
	const float OfferTime = 8.f;
	const TCHAR* ClosedCrate = TEXT("/Game/ZS_Imported/Props/AmmoBoxes/SM_Ammobox_40mm_Grenade_Closed.SM_Ammobox_40mm_Grenade_Closed");
	const TCHAR* OpenCrate = TEXT("/Game/ZS_Imported/Props/AmmoBoxes/SM_Ammobox_40mm_Grenade.SM_Ammobox_40mm_Grenade");

	/** Fit a crate mesh to ~120 cm long, sitting on the floor (the box's origin is 28 cm up). */
	void FitCrate(UStaticMeshComponent* C, const TCHAR* Path)
	{
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M) { return; }
		C->SetStaticMesh(M);
		C->SetMaterial(0, nullptr);
		const FBox B = M->GetBoundingBox();
		const float S = 120.f / FMath::Max(1.f, FMath::Max(B.GetSize().X, B.GetSize().Y));
		C->SetRelativeScale3D(FVector(S));
		C->SetRelativeLocation(FVector(-B.GetCenter().X * S, -B.GetCenter().Y * S, -28.f - B.Min.Z * S));
	}
}

AZSMysteryBox::AZSMysteryBox()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetBoxExtent(FVector(55.f, 30.f, 28.f));
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ZS_TRACE_INTERACT, ECR_Block);
	RootComponent = Collision;

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Crate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crate"));
	Crate->SetupAttachment(Collision);
	Crate->SetStaticMesh(Cube);
	Crate->SetRelativeScale3D(FVector(1.1f, 0.6f, 0.5f));
	Crate->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Lid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid"));
	Lid->SetupAttachment(Collision);
	Lid->SetStaticMesh(Cube);
	Lid->SetRelativeLocation(FVector(0.f, 0.f, 28.f));
	Lid->SetRelativeScale3D(FVector(1.14f, 0.64f, 0.06f));
	Lid->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The beacon: a tall faint light column you can see from across the map.
	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	Beam->SetupAttachment(Collision);
	Beam->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	Beam->SetRelativeLocation(FVector(0.f, 0.f, 600.f));
	Beam->SetRelativeScale3D(FVector(0.35f, 0.35f, 12.f));
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetCastShadow(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Collision);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(20.f);
	Glow->SetAttenuationRadius(500.f);
	Glow->SetLightColor(FLinearColor(0.35f, 0.6f, 1.f));
	Glow->SetCastShadows(false);

	GunPivot = CreateDefaultSubobject<USceneComponent>(TEXT("GunPivot"));
	GunPivot->SetupAttachment(Collision);
	GunPivot->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	GunPivot->SetRelativeScale3D(FVector(1.4f));   // guns read better a bit bigger floating over the box
}

void AZSMysteryBox::BeginPlay()
{
	Super::BeginPlay();
	// The owner's military crate if it's there (the plain cube + lid otherwise).
	if (LoadObject<UStaticMesh>(nullptr, ClosedCrate, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		FitCrate(Crate, ClosedCrate);
		Lid->SetVisibility(false);
	}
	UMaterialInterface* Paint = Lid->IsVisible() ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ZS/Materials/M_ZS_Paint.M_ZS_Paint"), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	UMaterialInterface* GlowMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ZS/Materials/M_ZS_Glow.M_ZS_Glow"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Paint)
	{
		UMaterialInstanceDynamic* Wood = UMaterialInstanceDynamic::Create(Paint, this);
		Wood->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.11f, 0.065f, 0.03f));
		Wood->SetScalarParameterValue(TEXT("Roughness"), 0.8f);
		Crate->SetMaterial(0, Wood);
		UMaterialInstanceDynamic* LidMat = UMaterialInstanceDynamic::Create(Paint, this);
		LidMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.08f, 0.05f, 0.025f));
		LidMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.08f, 0.05f, 0.025f));
		Lid->SetMaterial(0, LidMat);
	}
	if (GlowMat)
	{
		UMaterialInstanceDynamic* B = UMaterialInstanceDynamic::Create(GlowMat, this);
		B->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.3f, 0.55f, 1.f));
		B->SetScalarParameterValue(TEXT("Opacity"), 0.12f);
		Beam->SetMaterial(0, B);
	}
	OpenSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/InfimaGames/TacticalFPSAnimations/Common/Audio/Melee/A_TFA_Melee_Whoosh_Cue.A_TFA_Melee_Whoosh_Cue"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

FText AZSMysteryBox::GetInteractPrompt() const
{
	if (State == EState::Offering)
	{
		return FText::Format(NSLOCTEXT("ZS", "BoxTake", "TAKE {0}"), ZSWeaponCatalog::DisplayName(OfferedId));
	}
	return FText::Format(NSLOCTEXT("ZS", "BoxBuy", "MYSTERY BOX  ${0}"), FText::AsNumber(Cost));
}

bool AZSMysteryBox::CanInteract(const AZSPlayerCharacter* Player) const
{
	return State != EState::Rolling;
}

void AZSMysteryBox::Interact(AZSPlayerCharacter* Player)
{
	if (State == EState::Offering)
	{
		if (UZSWeaponComponent* W = Player ? Player->GetWeaponComponent() : nullptr)
		{
			W->GiveWeapon(OfferedId, OfferedAttachments);
		}
		Close();
		return;
	}
	if (State != EState::Idle)
	{
		return;
	}
	AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>();
	if (!GS || GS->RunCash < Cost)
	{
		AZSHUD::Toast(Player, FText::Format(NSLOCTEXT("ZS", "BoxPoor", "NEED ${0} MORE"), FText::AsNumber(Cost - (GS ? GS->RunCash : 0))));
		return;
	}
	GS->RunCash -= Cost;
	if (!Lid->IsVisible()) { FitCrate(Crate, OpenCrate); }
	State = EState::Rolling;
	StateTime = 0.f;
	NextSwap = 0.f;
	Glow->SetIntensity(60.f);
	if (OpenSound) { UGameplayStatics::PlaySoundAtLocation(this, OpenSound, GetActorLocation(), 1.f, 0.6f); }

	// Roll the result now; the spin is just a show.
	const TArray<ZSWeaponCatalog::FEntry>& All = ZSWeaponCatalog::All();
	const UZSWeaponComponent* Held = Player ? Player->GetWeaponComponent() : nullptr;
	TArray<const ZSWeaponCatalog::FEntry*> Pool;
	for (const ZSWeaponCatalog::FEntry& E : All)
	{
		if (!Held || !Held->HasWeapon(E.ItemId)) { Pool.Add(&E); }
	}
	if (Pool.Num() == 0) { for (const ZSWeaponCatalog::FEntry& E : All) { Pool.Add(&E); } }
	const ZSWeaponCatalog::FEntry* Pick = Pool[FMath::RandRange(0, Pool.Num() - 1)];
	OfferedId = Pick->ItemId;
	OfferedAttachments.Reset();
	// Up to three random attachments, one per slot.
	TArray<FName> Options = Pick->Attachments;
	const int32 Want = FMath::RandRange(0, 3);
	TSet<uint8> UsedSlots;
	while (OfferedAttachments.Num() < Want && Options.Num() > 0)
	{
		const FName A = Options[FMath::RandRange(0, Options.Num() - 1)];
		Options.Remove(A);
		if (const ZSWeaponCatalog::FAttachment* Att = ZSWeaponCatalog::FindAttachment(A); Att && !UsedSlots.Contains(uint8(Att->Slot)))
		{
			UsedSlots.Add(uint8(Att->Slot));
			OfferedAttachments.Add(A);
		}
	}
}

void AZSMysteryBox::ShowGun(FName ItemId)
{
	ZSWeaponDisplay::Build(this, GunPivot, ItemId, GunParts, FTransform::Identity);
}

void AZSMysteryBox::Close()
{
	State = EState::Idle;
	StateTime = 0.f;
	for (USceneComponent* C : GunParts) { if (C) { C->DestroyComponent(); } }
	GunParts.Reset();
	Glow->SetIntensity(20.f);
	if (!Lid->IsVisible()) { FitCrate(Crate, ClosedCrate); }
}

void AZSMysteryBox::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StateTime += DeltaSeconds;
	// Lid: open while rolling / offering.
	const float Open = State == EState::Idle ? 0.f : FMath::Clamp(StateTime * 4.f, 0.f, 1.f);
	Lid->SetRelativeLocationAndRotation(FVector(-30.f * Open, 0.f, 28.f + 30.f * Open), FRotator(0.f, 0.f, 0.f) + FRotator(-70.f * Open, 0.f, 0.f));
	if (State == EState::Rolling)
	{
		// Guns flick past, slowing down, rising out of the box.
		GunPivot->SetRelativeLocation(FVector(0.f, 0.f, 40.f + 45.f * FMath::Min(1.f, StateTime / RollTime)));
		NextSwap -= DeltaSeconds;
		if (NextSwap <= 0.f)
		{
			const TArray<ZSWeaponCatalog::FEntry>& All = ZSWeaponCatalog::All();
			ShowGun(All[FMath::RandRange(0, All.Num() - 1)].ItemId);
			NextSwap = FMath::Lerp(0.08f, 0.45f, FMath::Square(StateTime / RollTime));
		}
		if (StateTime >= RollTime)
		{
			State = EState::Offering;
			StateTime = 0.f;
			ShowGun(OfferedId);
		}
	}
	else if (State == EState::Offering)
	{
		GunPivot->AddLocalRotation(FRotator(0.f, 40.f * DeltaSeconds, 0.f));
		// Sinks back in as time runs out.
		GunPivot->SetRelativeLocation(FVector(0.f, 0.f, 85.f - 45.f * FMath::Clamp((StateTime - OfferTime + 1.5f) / 1.5f, 0.f, 1.f)));
		if (StateTime >= OfferTime)
		{
			Close();
		}
	}
}
