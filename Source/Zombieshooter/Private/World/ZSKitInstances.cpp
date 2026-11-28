#include "World/ZSKitInstances.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AZSKitInstances::AZSKitInstances()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
}

void AZSKitInstances::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void AZSKitInstances::BeginPlay()
{
	Super::BeginPlay();
	if (Components.Num() == 0)
	{
		Rebuild();   // construction doesn't run again when a saved level loads in game
	}
}

void AZSKitInstances::Rebuild()
{
	for (UInstancedStaticMeshComponent* C : Components)
	{
		if (C) { C->DestroyComponent(); }
	}
	Components.Reset();

	for (const FZSKitBatch& Batch : Batches)
	{
		if (!Batch.Mesh || Batch.Instances.Num() == 0)
		{
			continue;
		}
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		C->SetupAttachment(RootComponent);
		C->SetMobility(EComponentMobility::Movable);   // built at runtime (with Static + HISM the map hung the GPU in Nanite shadow culling)
		C->SetStaticMesh(Batch.Mesh);
		C->SetCastShadow(Batch.bCastShadow);
		// The kit's floor tiles have rusty-orange metal studs on their own material slot; across a whole roof or
		// compound floor they read as orange junk scattered everywhere. Worn steel instead: dark, subtle bolt caps.
		if (Batch.Mesh->GetName().Contains(TEXT("Floor")))
		{
			static const TCHAR* SteelPath = TEXT("/Game/IndustrialKit/SM_Floor_Concrete_2x2/Materials/M_Steel_Worn.M_Steel_Worn");
			UMaterialInterface* Steel = LoadObject<UMaterialInterface>(nullptr, SteelPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
			const TArray<FStaticMaterial>& Slots = Batch.Mesh->GetStaticMaterials();
			for (int32 i = 0; Steel && i < Slots.Num(); ++i)
			{
				if (Slots[i].MaterialInterface && Slots[i].MaterialInterface->GetName().Contains(TEXT("Rust")))
				{
					C->SetMaterial(i, Steel);
				}
			}
		}
		if (Batch.bCollide)
		{
			C->SetCollisionProfileName(TEXT("BlockAll"));
		}
		else
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCanEverAffectNavigation(false);
		}
		C->RegisterComponent();
		C->AddInstances(Batch.Instances, false, true);   // world space
		Components.Add(C);
	}
}
