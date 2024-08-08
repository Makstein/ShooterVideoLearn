// Fill out your copyright notice in the Description page of Project Settings.


#include "item.h"

#include "ShooterCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Curves/CurveVector.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"

// Sets default values
AItem::AItem():
	ItemName("Default"),
	ItemCount(0),
	ItemRarity(EItemRarity::Eir_Common),
	ItemState(EItemState::EIS_Pickup),
	// Item interp variables
	ItemInterpStartLocation(FVector::Zero()),
	CameraTargetLocation(FVector::Zero()),
	bInterping(false),
	ZCurveTime(0.7f),
	ItemInterpX(0.f),
	ItemInterpY(0.f),
	InterpInitialYawOffset(0.f),
	ItemType(EItemType::EIT_Max),
	InterpLocIndex(0),
	MaterialIndex(0),
	bCanChangeCustomDepth(true),
	// Dynamic Material parameters
	GlowAmount(150.f),
	FresnelExponent(3.f),
	FresnelReflectFraction(4.f),
	PulseCurveTime(5.f)
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
			ItemMesh->SetEnableGravity(false);
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
			PickupWidget->SetVisibility(false);
			// Set Mesh properties
			ItemMesh->SetSimulatePhysics(false);
			ItemMesh->SetVisibility(true);
			ItemMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			ItemMesh->SetEnableGravity(false);
			ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// Set AreaSphere properties
			AreaSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
			AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// Set CollisionBox properties
			CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
			CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
		}
	case EItemState::EIS_Falling:
		{
			// Set Mesh properties
			ItemMesh->SetSimulatePhysics(true);
			ItemMesh->SetEnableGravity(true);
			ItemMesh->SetVisibility(true);
			ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			ItemMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			ItemMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
			// Set AreaSphere properties
			AreaSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
			AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// Set CollisionBox properties
			CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
			CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
		}
	case EItemState::EIS_EquipInterping:
		{
			PickupWidget->SetVisibility(false);
			// Set Mesh properties
			ItemMesh->SetSimulatePhysics(false);
			ItemMesh->SetVisibility(true);
			ItemMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			ItemMesh->SetEnableGravity(false);
			ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// Set AreaSphere properties
			AreaSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
			AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// Set CollisionBox properties
			CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
			CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
		}
	default: break;
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

	// Set custom depth to disable by default
	InitializeCustomDepth();

	StartPulseTimer();
}

void AItem::FinishInterping()
{
	bInterping = false;

	if (!Character) return;

	Character->IncrementInterpLocItemCount(InterpLocIndex, -1);
	Character->GetPickupItem(this);
	SetItemState(EItemState::EIS_PickedUp);

	SetActorScale3D(FVector::One());

	DisableGlowMaterial();
	bCanChangeCustomDepth = true;
	DisableCustomDepth();
} 

void AItem::ItemInterp(const float DeltaTime)
{
	if (!bInterping) return;

	if (!Character || !ItemZCurve) return;

	// Elapsed Time Since Started Interping
	const float ElapsedTime = GetWorldTimerManager().GetTimerElapsed(ItemInterpTimer);
	// Get curve value corresponding to elapsed time
	const float CurveValue = ItemZCurve->GetFloatValue(ElapsedTime);
	// Get the item's initial location when curve started
	FVector ItemLocation = ItemInterpStartLocation;
	// Get location in front of camera
	const FVector CameraInterpLocation{ GetInterpLocation() };
	// Vector from item to camera interp location, X and Y are zeroed out
	const FVector ItemToCamera = FVector(0.f, 0.f, (CameraInterpLocation - ItemLocation).Z);
	// Scale factor to multiply with curve value
	const float DeltaZ = ItemToCamera.Size();

	const FVector CurrentLocation{ GetActorLocation() };
	const float InterpXValue = FMath::FInterpTo(CurrentLocation.X, CameraInterpLocation.X, DeltaTime, 30.f);
	const float InterpYValue = FMath::FInterpTo(CurrentLocation.Y, CameraInterpLocation.Y, DeltaTime, 30.f);

	// Set X and Y of ItemLocation to Interpolated values
	ItemLocation.Set(InterpXValue, InterpYValue, ItemInterpStartLocation.Z);

	// Add curve value to the Z component of the initial location(scaled by DeltaZ)
	ItemLocation.Z += CurveValue * DeltaZ;
	SetActorLocation(ItemLocation, true, nullptr, ETeleportType::TeleportPhysics);

	const FRotator CameraRotation{ Character->GetFollowCamera()->GetComponentRotation() };
	const FRotator ItemRotation{ 0.f, CameraRotation.Yaw + InterpInitialYawOffset, 0.f };
	SetActorRotation(ItemRotation, ETeleportType::TeleportPhysics);

	if (!ItemScaleCurve) return;
	const float ScaleCurveValue = ItemScaleCurve->GetFloatValue(ElapsedTime);
	SetActorScale3D(FVector{ ScaleCurveValue, ScaleCurveValue, ScaleCurveValue });
}

FVector AItem::GetInterpLocation()
{
	if (Character == nullptr) return FVector();

	switch (ItemType)
	{
	case EItemType::EIT_Ammo:
		return Character->GetInterpLocation(InterpLocIndex).SceneComp->GetComponentLocation();

	case EItemType::EIT_Weapon:
		return Character->GetInterpLocation(0).SceneComp->GetComponentLocation();

	default: ;
	}

	return FVector();
}

void AItem::PlayPickupSound()
{
	if (Character && Character->ShouldPlayPickupSound())
	{
		Character->StartPickupSoundTimer();
		if (PickupSound)
		{
			UGameplayStatics::PlaySound2D(this, PickupSound);
		}
	}
}

void AItem::EnableCustomDepth()
{
	if (!bCanChangeCustomDepth) return;
	ItemMesh->SetRenderCustomDepth(true);
}

void AItem::DisableCustomDepth()
{
	if (!bCanChangeCustomDepth) return;
	ItemMesh->SetRenderCustomDepth(false);
}

void AItem::InitializeCustomDepth()
{
	DisableCustomDepth();
}

void AItem::OnConstruction(const FTransform& Transform)
{
	if (MaterialInstance)
	{
		DynamicMaterialInstance = UMaterialInstanceDynamic::Create(MaterialInstance, this);
		ItemMesh->SetMaterial(MaterialIndex, DynamicMaterialInstance);
	}
	EnableGlowMaterial();
}

void AItem::EnableGlowMaterial()
{
	if (DynamicMaterialInstance)
	{
		DynamicMaterialInstance->SetScalarParameterValue(TEXT("GlowBlendAlpha"), 0);
	}
}

void AItem::UpdatePulse()
{
	float ElapsedTime;
	FVector CurveValue{};
	switch (ItemState)
	{
	case EItemState::EIS_Pickup:
		if (PulseCurve)
		{
			ElapsedTime = GetWorldTimerManager().GetTimerElapsed(PulseTimer);
			CurveValue = PulseCurve->GetVectorValue(ElapsedTime);
		}
		break;
	case EItemState::EIS_EquipInterping:
		if (InterpPulseCurve)
		{
			ElapsedTime = GetWorldTimerManager().GetTimerElapsed(ItemInterpTimer);
			CurveValue = InterpPulseCurve->GetVectorValue(ElapsedTime);
		}
		break;
	default: ;
	}

	if (DynamicMaterialInstance)
	{
		DynamicMaterialInstance->SetScalarParameterValue(TEXT("GlowAmount"), CurveValue.X * GlowAmount);
		DynamicMaterialInstance->SetScalarParameterValue(TEXT("FresnelExponent"), CurveValue.Y * FresnelExponent);
		DynamicMaterialInstance->SetScalarParameterValue(TEXT("FresnelReflectFraction"), CurveValue.Z * FresnelReflectFraction);
	}
}

void AItem::DisableGlowMaterial()
{
	if (DynamicMaterialInstance)
	{
		DynamicMaterialInstance->SetScalarParameterValue(TEXT("GlowBlendAlpha"), 1);
	}
}

void AItem::PlayEquipSound()
{
	if (Character && Character->ShouldPlayEquipSound())
	{
		Character->StartEquipSoundTimer();
		if (EquipSound)
		{
			UGameplayStatics::PlaySound2D(this, EquipSound);
		}
	}
}

// Called every frame
void AItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ItemInterp(DeltaTime);

	// Get the curve values from the PulseCurve and set the dynamic material parameters
	UpdatePulse();
}

void AItem::ResetPulseTimer()
{
	StartPulseTimer();
}

void AItem::StartPulseTimer()
{
	if (ItemState == EItemState::EIS_Pickup)
	{
		GetWorldTimerManager().SetTimer(PulseTimer, this, &AItem::ResetPulseTimer, PulseCurveTime);
	}
}

void AItem::SetItemState(const EItemState State)
{
	ItemState = State;
	SetItemProperties(State);
}

void AItem::StartItemCurve(AShooterCharacter* Char)
{
	// Store a handle to the Character
	Character = Char;

	InterpLocIndex = Character->GetInterpLocationIndex();
	Character->IncrementInterpLocItemCount(InterpLocIndex, 1);

	PlayPickupSound();

	// Get initial location
	ItemInterpStartLocation = GetActorLocation();
	bInterping = true;
	SetItemState(EItemState::EIS_EquipInterping);
	
	GetWorldTimerManager().ClearTimer(PulseTimer);
	GetWorldTimerManager().SetTimer(ItemInterpTimer, this, &AItem::FinishInterping, ZCurveTime);

	const double CameraRotationYaw{ Character->GetFollowCamera()->GetComponentRotation().Yaw };
	const double ItemRotationYaw{ GetActorRotation().Yaw };
	InterpInitialYawOffset = ItemRotationYaw - CameraRotationYaw;

	bCanChangeCustomDepth = false;
}
