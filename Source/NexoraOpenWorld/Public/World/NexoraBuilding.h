#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "NexoraBuilding.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

UENUM(BlueprintType)
enum class EBuildingType : uint8
{
	Shop      UMETA(DisplayName = "Shop / Market"),
	Hospital  UMETA(DisplayName = "Hospital / Clinic"),
	QuestHub  UMETA(DisplayName = "Quest Guild / Hub"),
	Garage    UMETA(DisplayName = "Vehicle Garage"),
	SafeHouse UMETA(DisplayName = "Player Safe House")
};

UCLASS()
class NEXORAOPENWORLD_API ANexoraBuilding : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	ANexoraBuilding();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	UStaticMeshComponent* BuildingMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	UBoxComponent* EntranceTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	EBuildingType BuildingType = EBuildingType::Shop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	FText BuildingName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	FText BuildingDescription;

	UFUNCTION(BlueprintImplementableEvent, Category = "Building")
	void OnBuildingEntered(AActor* EnteringActor);

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
