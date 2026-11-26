#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "NexoraAnimInstance.generated.h"

class ANexoraCharacter;
class UCharacterMovementComponent;

UCLASS()
class NEXORAOPENWORLD_API UNexoraAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	ANexoraCharacter* OwnerCharacter = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	UCharacterMovementComponent* MovementComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float GroundSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float Direction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bShouldMove = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bIsSprinting = false;
};
