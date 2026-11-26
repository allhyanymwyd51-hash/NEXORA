#include "Core/NexoraGameMode.h"
#include "Character/NexoraCharacter.h"

ANexoraGameMode::ANexoraGameMode()
{
	DefaultPawnClass = ANexoraCharacter::StaticClass();
}
