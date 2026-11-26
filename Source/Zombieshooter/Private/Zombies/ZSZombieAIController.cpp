#include "Zombies/ZSZombieAIController.h"
#include "Navigation/PathFollowingComponent.h"

AZSZombieAIController::AZSZombieAIController()
{
	bWantsPlayerState = false;
	bSetControlRotationFromPawnOrientation = true;
}

bool AZSZombieAIController::ChaseActor(AActor* Goal, float AcceptanceRadius)
{
	if (!Goal)
	{
		return false;
	}
	FAIMoveRequest Request(Goal);
	Request.SetAcceptanceRadius(AcceptanceRadius);
	Request.SetUsePathfinding(true);
	Request.SetAllowPartialPath(true);
	Request.SetReachTestIncludesAgentRadius(true);
	Request.SetCanStrafe(false);
	const FPathFollowingRequestResult Result = MoveTo(Request);
	return Result.Code != EPathFollowingRequestResult::Failed;
}
