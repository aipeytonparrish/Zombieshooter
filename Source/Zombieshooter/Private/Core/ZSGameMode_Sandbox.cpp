#include "Core/ZSGameMode_Sandbox.h"
#include "Player/ZSPlayerCharacter.h"
#include "Player/ZSPlayerController.h"

AZSGameMode_Sandbox::AZSGameMode_Sandbox()
{
	DefaultPawnClass = AZSPlayerCharacter::StaticClass();
	PlayerControllerClass = AZSPlayerController::StaticClass();
}
