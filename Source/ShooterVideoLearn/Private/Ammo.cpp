// Fill out your copyright notice in the Description page of Project Settings.


#include "Ammo.h"

#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"


// Sets default values
AAmmo::AAmmo()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create Ammo Mesh
	AmmoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ammo Mesh"));
	SetRootComponent(AmmoMesh);

	GetCollisionBox()->SetupAttachment(GetRootComponent());
	GetPickupWidget()->SetupAttachment(GetRootComponent());
	GetAreaSphere()->SetupAttachment(GetRootComponent());
}

// Called when the game starts or when spawned
void AAmmo::BeginPlay()
{
	Super::BeginPlay();
	
}

void AAmmo::SetItemProperties(const EItemState State)
{
	Super::SetItemProperties(State);

	switch (State)
	{
	case EItemState::EIS_Pickup:
		{
			// Set Mesh properties
			AmmoMesh->SetSimulatePhysics(false);
			AmmoMesh->SetVisibility(true);
			AmmoMesh->SetEnableGravity(false);
			AmmoMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			AmmoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
		}
	case EItemState::EIS_Equipped:
		{
			// Set Mesh properties
			AmmoMesh->SetSimulatePhysics(false);
			AmmoMesh->SetVisibility(true);
			AmmoMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			AmmoMesh->SetEnableGravity(false);
			AmmoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
		}
	case EItemState::EIS_Falling:
		{
			// Set Mesh properties
			AmmoMesh->SetSimulatePhysics(true);
			AmmoMesh->SetEnableGravity(true);
			AmmoMesh->SetVisibility(true);
			AmmoMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			AmmoMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			AmmoMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
			break;
		}
	case EItemState::EIS_EquipInterping:
		{
			// Set Mesh properties
			AmmoMesh->SetSimulatePhysics(false);
			AmmoMesh->SetVisibility(true);
			AmmoMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			AmmoMesh->SetEnableGravity(false);
			AmmoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
		}
	default: break;
	}
}

// Called every frame

void AAmmo::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

