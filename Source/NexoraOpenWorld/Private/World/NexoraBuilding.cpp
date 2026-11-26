#include "World/NexoraBuilding.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Character/NexoraCharacter.h"

ANexoraBuilding::ANexoraBuilding()
{
	PrimaryActorTick.bCanEverTick = false;

	BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
	RootComponent = BuildingMesh;

	EntranceTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EntranceTrigger"));
	EntranceTrigger->SetupAttachment(RootComponent);
	EntranceTrigger->SetBoxExtent(FVector(200.f, 200.f, 150.f));
	EntranceTrigger->SetCollisionProfileName(TEXT("Trigger"));

	BuildingName = FText::FromString(TEXT("City Building"));
	BuildingDescription = FText::FromString(TEXT("An interactive building in the open world."));
}

void ANexoraBuilding::BeginPlay()
{
	Super::BeginPlay();
	if (EntranceTrigger)
	{
		EntranceTrigger->OnComponentBeginOverlap.AddDynamic(this, &ANexoraBuilding::HandleOverlapBegin);
	}
}

void ANexoraBuilding::HandleOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<ANexoraCharacter>(OtherActor))
	{
		OnBuildingEntered(OtherActor);
	}
}

void ANexoraBuilding::Interact_Implementation(AActor* Interactor)
{
	if (Cast<ANexoraCharacter>(Interactor))
	{
		OnBuildingEntered(Interactor);
	}
}

FText ANexoraBuilding::GetInteractionPrompt_Implementation() const
{
	return FText::Format(FText::FromString(TEXT("Press E to Enter {0}")), BuildingName);
}
