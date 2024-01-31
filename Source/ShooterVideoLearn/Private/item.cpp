// Fill out your copyright notice in the Description page of Project Settings.


#include "item.h"

#include "ShooterCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"

// Sets default values
AItem::AItem():
	ItemName("Default"),
	ItemCount(0),
	ItemRarity(EItemRarity::Eir_Common),
	ItemState(EItemState::EIS_Pickup)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ItemMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Item Mesh"));
	SetRootComponent(ItemMesh);

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision Box"));
	CollisionBox->SetupAttachment(ItemMesh);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("Pickup Widget"));
	PickupWidget->SetupAttachment(RootComponent);

	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Area Sphere"));
	AreaSphere->SetupAttachment(RootComponent);
}

void AItem::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                            const FHitResult& SweepResult)
{
	if (!OtherActor) return;

	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if (!ShooterCharacter) return;
	ShooterCharacter->IncrementOverlappedItemCount(1);
}

void AItem::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                               UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor) return;

	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if (!ShooterCharacter) return;
	ShooterCharacter->IncrementOverlappedItemCount(-1);
}

void AItem::SetActiveStars()
{
	for (int i = 0; i < 5; i++)
	{
		ActiveStars.Add(false);
	}

	for (int i = 0; i <= static_cast<uint8>(ItemRarity); i++)
	{
		ActiveStars[i] = true;
	}
}

void AItem::SetItemProperties(const EItemState State)
{
	switch (State)
	{
	case EItemState::EIS_Pickup:
		{
			// Set Mesh properties
			ItemMesh->SetSimulatePhysics(false);
			ItemMesh->SetVisibility(true);
			ItemMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// Set AreaSphere properties
			AreaSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
			AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			// Set CollisionBox properties
			CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
			CollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			break;
		}
	case EItemState::EIS_Equipped:
		{
			// Set Mesh properties
			ItemMesh->SetSimulatePhysics(false);
			ItemMesh->SetVisibility(true);
			ItemMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// Set AreaSphere properties
			AreaSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
			AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// Set CollisionBox properties
			CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
			CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
		}
	}
}

// Called when the game starts or when spawned
void AItem::BeginPlay()
{
	Super::BeginPlay();

	PickupWidget->SetVisibility(false);

	// Set ActiveStars Array based on Rarity
	SetActiveStars();

	// Set up overlapped for area sphere
	AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &AItem::OnSphereOverlap);
	AreaSphere->OnComponentEndOverlap.AddDynamic(this, &AItem::OnSphereEndOverlap);

	// Set Item Properties based on State
	SetItemProperties(ItemState);
}

// Called every frame
void AItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AItem::SetItemState(const EItemState State)
{
	ItemState = State;
	SetItemProperties(State);
}
