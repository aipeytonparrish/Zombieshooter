#include "Combat/ZSAimAssistSubsystem.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

void UZSAimAssistSubsystem::RegisterTarget(AActor* Actor, USceneComponent* Body, USceneComponent* Head, float Radius)
{
	if (!Actor)
	{
		return;
	}
	UnregisterTarget(Actor);
	FZSAimTarget& T = Targets.AddDefaulted_GetRef();
	T.Actor = Actor;
	T.Body = Body ? Body : Actor->GetRootComponent();
	T.Head = Head;
	T.Radius = Radius;
}

void UZSAimAssistSubsystem::UnregisterTarget(AActor* Actor)
{
	Targets.RemoveAll([Actor](const FZSAimTarget& T) { return !T.Actor.IsValid() || T.Actor.Get() == Actor; });
}

const TArray<FZSAimTarget>& UZSAimAssistSubsystem::GetTargets()
{
	Targets.RemoveAll([](const FZSAimTarget& T) { return !T.Actor.IsValid() || !T.Body.IsValid(); });
	return Targets;
}
