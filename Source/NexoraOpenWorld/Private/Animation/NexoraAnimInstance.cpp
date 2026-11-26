#include "Animation/NexoraAnimInstance.h"
#include "Character/NexoraCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void UNexoraAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	OwnerCharacter = Cast<ANexoraCharacter>(TryGetPawnOwner());
	if (OwnerCharacter)
	{
		MovementComponent = OwnerCharacter->GetCharacterMovement();
	}
}

void UNexoraAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerCharacter || !MovementComponent)
	{
		OwnerCharacter = Cast<ANexoraCharacter>(TryGetPawnOwner());
		if (OwnerCharacter)
		{
			MovementComponent = OwnerCharacter->GetCharacterMovement();
		}
		return;
	}

	FVector Velocity = OwnerCharacter->GetVelocity();
	Velocity.Z = 0.f;
	GroundSpeed = Velocity.Size();

	bShouldMove = GroundSpeed > 3.0f && MovementComponent->GetCurrentAcceleration() != FVector::ZeroVector;
	bIsFalling = MovementComponent->IsFalling();
	bIsSprinting = GroundSpeed > 450.f;

	if (GroundSpeed > 0.f)
	{
		FRotator ActorRot = OwnerCharacter->GetActorRotation();
		Direction = CalculateDirection(Velocity, ActorRot);
	}
	else
	{
		Direction = 0.f;
	}
}
