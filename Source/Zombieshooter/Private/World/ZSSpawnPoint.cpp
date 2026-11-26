#include "World/ZSSpawnPoint.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"

AZSSpawnPoint::AZSSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// Capsule the size of a zombie, so designers can see whether it fits.
	UCapsuleComponent* Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.f, 90.f);
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Capsule->ShapeColor = FColor(200, 40, 40);
	Capsule->SetHiddenInGame(true);
	RootComponent = Capsule;

#if WITH_EDITORONLY_DATA
	if (UArrowComponent* Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow")))
	{
		Arrow->SetupAttachment(Capsule);
		Arrow->ArrowColor = FColor(200, 40, 40);
	}
	if (UBillboardComponent* Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite")))
	{
		Sprite->SetupAttachment(Capsule);
	}
#endif
}
