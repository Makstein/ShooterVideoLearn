// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterCharacter.h"

#include "Ammo.h"
#include "BulletHitInterface.h"
#include "Enemy.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "item.h"
#include "Weapon.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "ShooterVideoLearn/AmmoType.h"
#include "Sound/SoundCue.h"

// Sets default values
AShooterCharacter::AShooterCharacter() :
	BaseTurnRate(.5f),
	BaseLookUpRate(.5f),
	HipTurnRate(1.f),
	HipLookUpRate(1.f),
	AimingTurnRate(.6f),
	AimingLookUpRate(.6f),
	bAiming(false),
	CameraDefaultFOV(0.f),
	CameraZoomedFOV(25.f),
	CameraCurrentFOV(0.f),
	ZoomInterpSpeed(20.f),
	// 准星散布
	CrosshairSpreadMultiplier(0.f),
	CrosshairVelocityFactor(0.f),
	CrosshairInAirFactor(0.f),
	CrosshairAimFactor(0.f),
	CrosshairShootingFactor(0.f),
	// 自动射击
	bFireButtonPressed(false),
	bShouldFire(true),
	// 射击参数
	ShootTimeDuration(0.05f),
	bFiringBullet(false),
	bShouldTraceForItems(false),
	// Camera interp location variables
	CameraInterpDistance(250.f),
	CameraInterpElevation(65.f),
	// Starting ammo amounts
	Starting9MMAmmo(50),
	StartingARAmmo(150),
	// Combat variables
	CombatState(ECombatState::ECS_Unoccupied),
	bCrouching(false),
	BaseMovementSpeed(650.f),
	CrouchMovementSpeed(300.f),
	StandingCapsuleHalfHeight(88.f),
	CrouchingCapsuleHalfHeight(40.f),
	BaseGroundFriction(2.f),
	CrouchingGroundFriction(100.f),
	bAimingButtonPressed(false),
	// Pickup sound properties
	bShouldPlayPickupSound(true),
	bShouldPlayEquipSound(true),
	PickupSoundResetTime(0.2f),
	EquipSoundResetTime(0.2f),
	// Icon animation properties
	HighlightedSlot(-1),
	Health(100.f),
	MaxHealth(100.f),
	StunChance(.25f)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 180.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach camera to end of the boom
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.0f, 0.f);
	GetCharacterMovement()->AirControl = 0.33f;
	GetCharacterMovement()->JumpZVelocity = 700.f;

	HandSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("HandSceneComp"));

	// Create interpolation components
	WeaponInterpComp = CreateDefaultSubobject<USceneComponent>(TEXT("Weapon Interpolation Component"));
	WeaponInterpComp->SetupAttachment(GetFollowCamera());

	InterpComp1 = CreateDefaultSubobject<USceneComponent>(TEXT("Interpolation Component 1"));
	InterpComp1->SetupAttachment(GetFollowCamera());

	InterpComp2 = CreateDefaultSubobject<USceneComponent>(TEXT("Interpolation Component 2"));
	InterpComp2->SetupAttachment(GetFollowCamera());

	InterpComp3 = CreateDefaultSubobject<USceneComponent>(TEXT("Interpolation Component 3"));
	InterpComp3->SetupAttachment(GetFollowCamera());

	InterpComp4 = CreateDefaultSubobject<USceneComponent>(TEXT("Interpolation Component 4"));
	InterpComp4->SetupAttachment(GetFollowCamera());

	InterpComp5 = CreateDefaultSubobject<USceneComponent>(TEXT("Interpolation Component 5"));
	InterpComp5->SetupAttachment(GetFollowCamera());

	InterpComp6 = CreateDefaultSubobject<USceneComponent>(TEXT("Interpolation Component 6"));
	InterpComp6->SetupAttachment(GetFollowCamera());
}

float AShooterCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
                                    class AController* EventInstigator, AActor* DamageCauser)
{
	if (Health - DamageAmount <= 0.f)
	{
		Health = 0.f;
	}
	else
	{
		Health -= DamageAmount;
	}
	return DamageAmount;
}

// Called when the game starts or when spawned
void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (UEnhancedInputLocalPlayerSubsystem* InputSystem = LocalPlayer->GetSubsystem<
		UEnhancedInputLocalPlayerSubsystem>())
	{
		if (!InputMapping.IsNull())
		{
			InputSystem->AddMappingContext(InputMapping.LoadSynchronous(), 100);
		}
	}

	CameraBoom->SocketOffset = FVector(0.f, 50.f, 70.f);

	if (FollowCamera)
	{
		CameraDefaultFOV = GetFollowCamera()->FieldOfView;
		CameraCurrentFOV = CameraDefaultFOV;
	}

	// Spawn the default weapon and equip it
	EquipWeapon(SpawnDefaultWeapon());
	Inventory.Add(EquippedWeapon);
	EquippedWeapon->SetSlotIndex(0);
	EquippedWeapon->DisableCustomDepth();
	EquippedWeapon->DisableGlowMaterial();
	EquippedWeapon->SetCharacter(this);

	InitializeAmmoMap();

	GetCharacterMovement()->MaxWalkSpeed = BaseMovementSpeed;

	InitializeInterpLocations();
}

// Called every frame
void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CameraInterpZoom(DeltaTime);
	SetLookRates();
	CalculateCrosshairSpread(DeltaTime);
	TraceForItems();
	// Interp capsule half height based on crouching/standing
	InterpCapsuleHalfHeight(DeltaTime);
}

// Called to bind functionality to input
void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);

	// Jump
	Input->BindAction(JumpInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::Jump);
	Input->BindAction(JumpInputAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	// Moving
	Input->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::CharacterMove);
	// Looking
	Input->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::CharacterLook);
	// Firing
	Input->BindAction(FireInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::CharacterFire);
	Input->BindAction(AimPressedAction, ETriggerEvent::Triggered, this, &AShooterCharacter::CharacterAim);
	// Interaction
	Input->BindAction(SelectInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::CharacterSelect);
	// Reload
	Input->BindAction(ReloadInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::CharacterReload);
	// Crouch
	Input->BindAction(CrouchInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::CharacterCrouch);

	Input->BindAction(FKeyInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::FKeyPressed);
	Input->BindAction(OneKeyInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::OneKeyPressed);
	Input->BindAction(TwoKeyInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::TwoKeyPressed);
	Input->BindAction(ThreeKeyInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::ThreeKeyPressed);
	Input->BindAction(FourKeyInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::FourKeyPressed);
	Input->BindAction(FiveKeyInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::FiveKeyPressed);
}

void AShooterCharacter::CharacterFire(const FInputActionValue& Value)
{
	bFireButtonPressed = true;
	if (Value.Get<float>() == 1)
	{
		FireWeapon();
	}
	else
	{
		bFireButtonPressed = false;
	}
}

void AShooterCharacter::CharacterAim(const FInputActionValue& Value)
{
	if (Value.Get<float>() == 1)
	{
		bAimingButtonPressed = !bAimingButtonPressed;
		if (CombatState != ECombatState::ECS_Reloading && CombatState != ECombatState::ECS_Equipping && CombatState !=
			ECombatState::ECS_Stunned)
		{
			bAimingButtonPressed ? Aim() : StopAim();
		}
	}

	// UE_LOG(LogTemp, Warning, TEXT("%f"), Value.Get<float>())
	// 为0时表示松开，为1时代表按下
}

void AShooterCharacter::CharacterSelect(const FInputActionValue& Value)
{
	if (Value.Get<float>() == 1)
	{
		if (CombatState != ECombatState::ECS_Unoccupied) return;
		if (!TraceHitItem) return;

		TraceHitItem->StartItemCurve(this, true);
		TraceHitItem = nullptr;
	}
}

void AShooterCharacter::CharacterReload(const FInputActionValue& Value)
{
	ReloadWeapon();
}

void AShooterCharacter::CharacterCrouch(const FInputActionValue& Value)
{
	if (GetCharacterMovement()->IsFalling()) return;

	if (Value.Get<float>() == 1)
	{
		bCrouching = !bCrouching;

		GetCharacterMovement()->MaxWalkSpeed = bCrouching ? CrouchMovementSpeed : BaseMovementSpeed;
		GetCharacterMovement()->GroundFriction = bCrouching ? CrouchingGroundFriction : BaseGroundFriction;
	}
}

bool AShooterCharacter::GetBeamEndLocation(const FVector& MuzzleSocketLocation, FHitResult& OutHitResult) const
{
	TraceUnderCrosshairs(OutHitResult);

	const FVector OutBeamLocation = OutHitResult.Location;

	const FVector WeaponTraceStart = MuzzleSocketLocation;
	const FVector StartToEnd{ OutBeamLocation - MuzzleSocketLocation };
	// 防止未检测到
	const FVector WeaponTraceEnd = MuzzleSocketLocation + StartToEnd * 1.25f;
	GetWorld()->LineTraceSingleByChannel(OutHitResult, WeaponTraceStart, WeaponTraceEnd, ECC_Visibility);
	if (!OutHitResult.bBlockingHit)
	{
		OutHitResult.Location = OutBeamLocation;
		return false;
	}

	return true;
}

void AShooterCharacter::CameraInterpZoom(const float DeltaTime)
{
	CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, bAiming ? CameraZoomedFOV : CameraDefaultFOV, DeltaTime,
	                                    ZoomInterpSpeed);
	GetFollowCamera()->SetFieldOfView(CameraCurrentFOV);
}

void AShooterCharacter::SetLookRates()
{
	BaseTurnRate = bAiming ? AimingTurnRate : HipTurnRate;
	BaseLookUpRate = bAiming ? AimingLookUpRate : HipLookUpRate;
}

void AShooterCharacter::CalculateCrosshairSpread(float DeltaTime)
{
	const FVector2D WalkSpeedRange{ 0.f, 600.f };
	const FVector2D VelocityRange{ 0.f, 1.f };
	FVector Velocity{ GetVelocity() };
	Velocity.Z = 0.f;

	CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityRange, Velocity.Size());

	CrosshairInAirFactor = GetCharacterMovement()->IsFalling()
		                       ? FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f)
		                       : FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);

	CrosshairAimFactor = bAiming
		                     ? FMath::FInterpTo(CrosshairAimFactor, 0.6f, DeltaTime, 30.f)
		                     : FMath::FInterpTo(CrosshairAimFactor, 0.f, DeltaTime, 30.f);

	CrosshairShootingFactor = bFiringBullet
		                          ? FMath::FInterpTo(CrosshairShootingFactor, 0.3f, DeltaTime, 60.f)
		                          : FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, 60.f);

	CrosshairSpreadMultiplier = 0.5f + CrosshairVelocityFactor + CrosshairInAirFactor - CrosshairAimFactor +
		CrosshairShootingFactor;
}

void AShooterCharacter::StartCrosshairBulletFire()
{
	bFiringBullet = true;

	GetWorldTimerManager().SetTimer(CrosshairShootTimer, this, &AShooterCharacter::FinishCrosshairBulletFire,
	                                ShootTimeDuration);
}

void AShooterCharacter::FinishCrosshairBulletFire()
{
	bFiringBullet = false;
}

void AShooterCharacter::StartFireTimer()
{
	if (EquippedWeapon == nullptr) return;
	CombatState = ECombatState::ECS_FireTimerInProgress;
	GetWorldTimerManager().SetTimer(AutoFireTimer, this, &AShooterCharacter::AutoFireReset,
	                                EquippedWeapon->GetAutoFireRate());
}

void AShooterCharacter::FireWeapon()
{
	if (EquippedWeapon == nullptr) return;
	if (CombatState != ECombatState::ECS_Unoccupied) return;

	if (!WeaponHasAmmo())
	{
		ReloadWeapon();
	}
	if (!WeaponHasAmmo()) return;

	PlayFireSound();
	SendBullet();
	PlayGunFireMontage();
	StartFireTimer();

	if (EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Pistol)
	{
		EquippedWeapon->StartSlideTimer();
	}

	if (EquippedWeapon)
	{
		EquippedWeapon->DecrementAmmo();
	}
}

void AShooterCharacter::AutoFireReset()
{
	if (CombatState == ECombatState::ECS_Stunned) return;
	CombatState = ECombatState::ECS_Unoccupied;
	if (EquippedWeapon == nullptr) return;
	if (!bFireButtonPressed) return;
	if (!EquippedWeapon->GetAutomatic()) return;
	if (WeaponHasAmmo())
	{
		FireWeapon();
	}
	else
	{
		// Reloading
		ReloadWeapon();
	}
}

bool AShooterCharacter::TraceUnderCrosshairs(FHitResult& OutHitResult) const
{
	FVector2d ViewPortSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewPortSize);
	}

	constexpr float CrosshairOffsetY = 0.f;
	const FVector2d CrosshairLocation(ViewPortSize.X / 2, ViewPortSize.Y / 2 - CrosshairOffsetY);
	FVector CrosshairWorldPosition, CrosshairWorldDirection;

	if (UGameplayStatics::DeprojectScreenToWorld(UGameplayStatics::GetPlayerController(this, 0),
	                                             CrosshairLocation,
	                                             CrosshairWorldPosition,
	                                             CrosshairWorldDirection))
	{
		const FVector Start{ CrosshairWorldPosition };
		const FVector End{ CrosshairWorldPosition + CrosshairWorldDirection * 50000.f };
		GetWorld()->LineTraceSingleByChannel(OutHitResult, Start, End, ECC_Visibility);

		if (OutHitResult.bBlockingHit)
		{
			return true;
		}
	}

	OutHitResult.Location = CrosshairWorldPosition + CrosshairWorldDirection * 50000.f;

	return false;
}

void AShooterCharacter::TraceForItems()
{
	if (!bShouldTraceForItems)
	{
		// No longer overlapping any items
		if (TraceHitItemLastFrame)
		{
			TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
			TraceHitItemLastFrame->DisableCustomDepth();
		}

		return;
	}

	if (FHitResult ItemTraceResult; TraceUnderCrosshairs(ItemTraceResult))
	{
		TraceHitItem = Cast<AItem>(ItemTraceResult.GetActor());
		if ([[maybe_unused]] const auto TraceHitWeapon = Cast<AWeapon>(TraceHitItem))
		{
			if (HighlightedSlot == -1)
			{
				// Not currently highlighting a slot, highlight one
				HighlightInventorySlot();
			}
		}
		else
		{
			if (HighlightedSlot != -1)
			{
				UnHighlightInventorySlot();
			}
		}

		if (TraceHitItem && TraceHitItem->GetItemState() == EItemState::EIS_EquipInterping)
		{
			TraceHitItem = nullptr;
		}

		if (TraceHitItem && TraceHitItem->GetPickupWidget())
		{
			TraceHitItem->GetPickupWidget()->SetVisibility(true);
			TraceHitItem->EnableCustomDepth();

			if (Inventory.Num() >= INVENTORY_CAPACITY)
			{
				TraceHitItem->SetCharacterInventoryFull(true);
			}
			else
			{
				TraceHitItem->SetCharacterInventoryFull(false);
			}
		}

		if (TraceHitItemLastFrame && TraceHitItemLastFrame != TraceHitItem)
		{
			TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
			TraceHitItemLastFrame->DisableCustomDepth();
		}

		TraceHitItemLastFrame = TraceHitItem;
	}
}

AWeapon* AShooterCharacter::SpawnDefaultWeapon() const
{
	if (!DefaultWeaponClass) return nullptr;

	return GetWorld()->SpawnActor<AWeapon>(DefaultWeaponClass);
}

void AShooterCharacter::EquipWeapon(AWeapon* WeaponToEquip, bool bSwapping)
{
	if (!WeaponToEquip) return;

	const USkeletalMeshSocket* HandSocket = GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (!HandSocket) return;
	HandSocket->AttachActor(WeaponToEquip, GetMesh());

	if (EquippedWeapon == nullptr)
	{
		// -1 = no equipped weapon yet. No need to play reversed animation
		EquipItemDelegate.Broadcast(-1, WeaponToEquip->GetSlotIndex());
	}
	else if (!bSwapping)
	{
		EquipItemDelegate.Broadcast(EquippedWeapon->GetSlotIndex(), WeaponToEquip->GetSlotIndex());
	}

	EquippedWeapon = WeaponToEquip;
	EquippedWeapon->SetItemState(EItemState::EIS_Equipped);
}

void AShooterCharacter::DropWeapon() const
{
	if (!EquippedWeapon) return;

	const FDetachmentTransformRules TransformRules(EDetachmentRule::KeepWorld, true);
	EquippedWeapon->GetItemMesh()->DetachFromComponent(TransformRules);
	EquippedWeapon->SetItemState(EItemState::EIS_Falling);
	EquippedWeapon->ThrowWeapon();
}

void AShooterCharacter::SwapWeapon(AWeapon* WeaponToSwap)
{
	if (Inventory.Num() - 1 >= EquippedWeapon->GetSlotIndex())
	{
		Inventory[EquippedWeapon->GetSlotIndex()] = WeaponToSwap;
		WeaponToSwap->SetSlotIndex(EquippedWeapon->GetSlotIndex());
	}

	DropWeapon();
	EquipWeapon(WeaponToSwap, true);
	TraceHitItem = nullptr;
	TraceHitItemLastFrame = nullptr;
}

void AShooterCharacter::InitializeAmmoMap()
{
	AmmoMap.Add(EAmmoType::EAT_9MM, Starting9MMAmmo);
	AmmoMap.Add(EAmmoType::EAT_AR, StartingARAmmo);
}

bool AShooterCharacter::WeaponHasAmmo() const
{
	if (EquippedWeapon == nullptr) return false;

	return EquippedWeapon->GetAmmo() > 0;
}

void AShooterCharacter::PlayFireSound() const
{
	// Play FireSound
	if (EquippedWeapon->GetFireSound())
	{
		UGameplayStatics::PlaySound2D(this, EquippedWeapon->GetFireSound());
	}
}

void AShooterCharacter::SendBullet()
{
	// Send bullet
	if (const USkeletalMeshSocket* BarrelSocket = EquippedWeapon->GetItemMesh()->GetSocketByName("BarrelSocket"))
	{
		const FTransform BarrelTransform = BarrelSocket->GetSocketTransform(EquippedWeapon->GetItemMesh());

		if (EquippedWeapon->GetMuzzleFlash())
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), EquippedWeapon->GetMuzzleFlash(), BarrelTransform);
		}

		// Weather have collision or not, there should have bullet trace, a little difference from the tutorial
		FHitResult BeamHitResult;
		if (GetBeamEndLocation(BarrelTransform.GetLocation(), BeamHitResult))
		{
			if (BeamHitResult.GetActor())
			{
				// If hit an enemy, try to call BulletHit interface
				if (IBulletHitInterface* BulletHitInterface = Cast<IBulletHitInterface>(BeamHitResult.GetActor()))
				{
					BulletHitInterface->BulletHit_Implementation(BeamHitResult);
				}
				if (AEnemy* HitEnemy = Cast<AEnemy>(BeamHitResult.GetActor()))
				{
					int32 Damage;
					bool bHeadshot;
					if (BeamHitResult.BoneName.ToString() == HitEnemy->GetHeadBone())
					{
						// Headshot
						Damage = EquippedWeapon->GetHeadShotDamage();
						bHeadshot = true;
					}
					else
					{
						// Body Shot
						Damage = EquippedWeapon->GetDamage();
						bHeadshot = false;
					}
					UGameplayStatics::ApplyDamage(BeamHitResult.GetActor(), Damage,
					                              GetController(), this, UDamageType::StaticClass());
					HitEnemy->ShowHitNumber(Damage, BeamHitResult.Location, bHeadshot);
				}
			}
			else if (ImpactParticles)
			{
				// Spawn default impact particles
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, BeamHitResult.Location);
			}
		}

		if (UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(), BeamParticles, BarrelTransform))
		{
			Beam->SetVectorParameter(FName("Target"), BeamHitResult.Location);
		}

		StartCrosshairBulletFire();
	}
}

void AShooterCharacter::PlayGunFireMontage() const
{
	// Play hip fire montage
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && HipFireMontage)
	{
		AnimInstance->Montage_Play(HipFireMontage);
		AnimInstance->Montage_JumpToSection(FName("StartFire"));
	}
}

void AShooterCharacter::ReloadWeapon()
{
	if (CombatState != ECombatState::ECS_Unoccupied) return;
	if (EquippedWeapon == nullptr) return;

	// Do we have the ammo of correct type?
	if (CarryingAmmo() && !EquippedWeapon->ClipIsFull())
	{
		if (bAiming)
		{
			bAiming = false;
			GetCharacterMovement()->MaxWalkSpeed = BaseMovementSpeed;
		}

		CombatState = ECombatState::ECS_Reloading;
		// const FName MontageSection(TEXT("Reload SMG"));
		// Play Reload Montage
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && ReloadMontage)
		{
			AnimInstance->Montage_Play(ReloadMontage);
			AnimInstance->Montage_JumpToSection(EquippedWeapon->GetReloadMontageSection());
		}
	}
}

void AShooterCharacter::FinishReloading()
{
	if (CombatState == ECombatState::ECS_Stunned) return;
	CombatState = ECombatState::ECS_Unoccupied;

	if (bAimingButtonPressed)
	{
		Aim();
	}

	if (EquippedWeapon == nullptr) return;

	const auto AmmoType{ EquippedWeapon->GetAmmoType() };

	// Update the ammo map
	if (AmmoMap.Contains(AmmoType))
	{
		int32 CarriedAmmo = AmmoMap[AmmoType];
		const int32 MagEmptySpace = EquippedWeapon->GetMagazineCapacity() - EquippedWeapon->GetAmmo();
		if (MagEmptySpace > CarriedAmmo)
		{
			EquippedWeapon->ReloadAmmo(CarriedAmmo);
			CarriedAmmo = 0;
			AmmoMap.Add(AmmoType, CarriedAmmo); // TMap make sure the key is unique
		}
		else
		{
			EquippedWeapon->ReloadAmmo(MagEmptySpace);
			CarriedAmmo -= MagEmptySpace;
			AmmoMap.Add(AmmoType, CarriedAmmo);
		}
	}

	if (bFireButtonPressed)
	{
		FireWeapon();
	}
}

void AShooterCharacter::FinishEquipping()
{
	if (CombatState == ECombatState::ECS_Stunned) return;
	CombatState = ECombatState::ECS_Unoccupied;
	if (bAimingButtonPressed)
	{
		Aim();
	}
}

bool AShooterCharacter::CarryingAmmo()
{
	if (EquippedWeapon == nullptr) return false;

	if (const auto AmmoType = EquippedWeapon->GetAmmoType(); AmmoMap.Contains(AmmoType))
	{
		return AmmoMap[AmmoType] > 0;
	}

	return false;
}

void AShooterCharacter::GrabClip()
{
	if (EquippedWeapon == nullptr) return;
	if (HandSceneComponent == nullptr) return;

	const int32 ClipBoneIndex{ EquippedWeapon->GetItemMesh()->GetBoneIndex(EquippedWeapon->GetClipBoneName()) };
	ClipTransform = EquippedWeapon->GetItemMesh()->GetBoneTransform(ClipBoneIndex);

	const FAttachmentTransformRules AttachmentRules(EAttachmentRule::KeepRelative, true);
	HandSceneComponent->AttachToComponent(GetMesh(), AttachmentRules, FName(TEXT("Hand_L")));
	HandSceneComponent->SetWorldTransform(ClipTransform);

	EquippedWeapon->SetMovingClip(true);
}

void AShooterCharacter::ReleaseClip()
{
	EquippedWeapon->SetMovingClip(false);
}

void AShooterCharacter::Jump()
{
	if (bCrouching)
	{
		bCrouching = false;
		GetCharacterMovement()->MaxWalkSpeed = BaseMovementSpeed;
		return;
	}

	Super::Jump();
}

void AShooterCharacter::InterpCapsuleHalfHeight(float DeltaTime) const
{
	const float TargetCapsuleHalfHeight = bCrouching ? CrouchingCapsuleHalfHeight : StandingCapsuleHalfHeight;
	const float InterpHalfHeight{
		FMath::FInterpTo(GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), TargetCapsuleHalfHeight, DeltaTime, 20.f)
	};
	// Negative value if we are crouching, positive if we are standing
	const float DeltaCapsuleHalfHeight{ InterpHalfHeight - GetCapsuleComponent()->GetScaledCapsuleHalfHeight() };
	const FVector MeshOffset{ 0.f, 0.f, -DeltaCapsuleHalfHeight };
	GetMesh()->AddLocalOffset(MeshOffset);
	GetCapsuleComponent()->SetCapsuleHalfHeight(InterpHalfHeight);
}

void AShooterCharacter::Aim()
{
	bAiming = true;
	GetCharacterMovement()->MaxWalkSpeed = CrouchMovementSpeed;
}

void AShooterCharacter::StopAim()
{
	bAiming = false;
	if (!bCrouching)
	{
		GetCharacterMovement()->MaxWalkSpeed = BaseMovementSpeed;
	}
}

void AShooterCharacter::PickupAmmo(AAmmo* Ammo)
{
	if (AmmoMap.Contains(Ammo->GetAmmoType()))
	{
		AmmoMap[Ammo->GetAmmoType()] += Ammo->GetItemCount();
	}

	if (EquippedWeapon->GetAmmoType() == Ammo->GetAmmoType())
	{
		if (EquippedWeapon->GetAmmo() == 0)
		{
			ReloadWeapon();
		}
	}

	Ammo->Destroy();
}

void AShooterCharacter::InitializeInterpLocations()
{
	const FInterpLocation WeaponLocation{ WeaponInterpComp, 0 };
	FInterpLocations.Add(WeaponLocation);

	const FInterpLocation InterpLoc1{ InterpComp1, 0 };
	FInterpLocations.Add(InterpLoc1);

	const FInterpLocation InterpLoc2{ InterpComp2, 0 };
	FInterpLocations.Add(InterpLoc2);

	const FInterpLocation InterpLoc3{ InterpComp3, 0 };
	FInterpLocations.Add(InterpLoc3);

	const FInterpLocation InterpLoc4{ InterpComp4, 0 };
	FInterpLocations.Add(InterpLoc4);

	const FInterpLocation InterpLoc5{ InterpComp5, 0 };
	FInterpLocations.Add(InterpLoc5);

	const FInterpLocation InterpLoc6{ InterpComp6, 0 };
	FInterpLocations.Add(InterpLoc6);
}

void AShooterCharacter::FKeyPressed()
{
	ExchangeInventoryItems(EquippedWeapon->GetSlotIndex(), 0);
}

void AShooterCharacter::OneKeyPressed()
{
	ExchangeInventoryItems(EquippedWeapon->GetSlotIndex(), 1);
}

void AShooterCharacter::TwoKeyPressed()
{
	ExchangeInventoryItems(EquippedWeapon->GetSlotIndex(), 2);
}

void AShooterCharacter::ThreeKeyPressed()
{
	ExchangeInventoryItems(EquippedWeapon->GetSlotIndex(), 3);
}

void AShooterCharacter::FourKeyPressed()
{
	ExchangeInventoryItems(EquippedWeapon->GetSlotIndex(), 4);
}

void AShooterCharacter::FiveKeyPressed()
{
	ExchangeInventoryItems(EquippedWeapon->GetSlotIndex(), 5);
}

void AShooterCharacter::ExchangeInventoryItems(int32 CurrentItemSlot, int32 NewItemSlot)
{
	if (CurrentItemSlot == NewItemSlot) return;
	if (NewItemSlot >= Inventory.Num()) return;
	if (CombatState != ECombatState::ECS_Unoccupied && CombatState != ECombatState::ECS_Equipping) return;

	if (bAiming)
	{
		StopAim();
	}

	const auto OldEquippedWeapon = EquippedWeapon;
	const auto NewWeapon = Cast<AWeapon>(Inventory[NewItemSlot]);
	EquipWeapon(NewWeapon);

	OldEquippedWeapon->SetItemState(EItemState::EIS_PickedUp);
	NewWeapon->SetItemState(EItemState::EIS_Equipped);

	CombatState = ECombatState::ECS_Equipping;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && EquipMontage)
	{
		AnimInstance->Montage_Play(EquipMontage, 1.f);
		AnimInstance->Montage_JumpToSection(FName("Equip"));
	}
	NewWeapon->PlayEquipSound(true);
}

int32 AShooterCharacter::GetEmptyInventorySlot()
{
	for (int32 i = 0; i < Inventory.Num(); i++)
	{
		if (Inventory[i] == nullptr)
		{
			return i;
		}
	}

	if (Inventory.Num() < INVENTORY_CAPACITY)
	{
		return Inventory.Num();
	}

	return -1; // Inventory is full
}

EPhysicalSurface AShooterCharacter::GetSurfaceType()
{
	FHitResult HitResult;
	const FVector Start{ GetActorLocation() };
	const FVector End{ Start + FVector(0.f, 0.f, -400.f) };
	FCollisionQueryParams QueryParams;
	QueryParams.bReturnPhysicalMaterial = true;

	GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);
	return UPhysicalMaterial::DetermineSurfaceType(HitResult.PhysMaterial.Get());
}

void AShooterCharacter::EndStun()
{
	CombatState = ECombatState::ECS_Unoccupied;
}

void AShooterCharacter::HighlightInventorySlot()
{
	const int32 EmptySlot{ GetEmptyInventorySlot() };
	HighlightIconDelegate.Broadcast(EmptySlot, true);
	HighlightedSlot = EmptySlot;
}

void AShooterCharacter::UnHighlightInventorySlot()
{
	HighlightIconDelegate.Broadcast(HighlightedSlot, false);
	HighlightedSlot = -1;
}

void AShooterCharacter::Stun()
{
	CombatState = ECombatState::ECS_Stunned;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
	}
}

int32 AShooterCharacter::GetInterpLocationIndex()
{
	int32 LowestIndex = 1;
	int32 LowestCount = INT_MAX;

	for (int32 i = 1; i < FInterpLocations.Num(); i++)
	{
		if (FInterpLocations[i].ItemCount < LowestCount)
		{
			LowestIndex = i;
			LowestCount = FInterpLocations[i].ItemCount;
		}
	}

	return LowestIndex;
}

void AShooterCharacter::IncrementInterpLocItemCount(int32 Index, int8 Amount)
{
	if (Amount < -1 || Amount > 1) return;

	if (FInterpLocations.Num() > Index)
	{
		FInterpLocations[Index].ItemCount += Amount;
	}
}

void AShooterCharacter::StartPickupSoundTimer()
{
	bShouldPlayPickupSound = false;
	GetWorldTimerManager().SetTimer(PickupSoundTimer, this, &AShooterCharacter::ResetPickupSoundTimer,
	                                PickupSoundResetTime, false);
}

void AShooterCharacter::StartEquipSoundTimer()
{
	bShouldPlayEquipSound = false;
	GetWorldTimerManager().SetTimer(EquipSoundTimer, this, &AShooterCharacter::ResetEquipSoundTimer,
	                                EquipSoundResetTime, false);
}

void AShooterCharacter::IncrementOverlappedItemCount(const int8 Amount)
{
	if (OverlappedItemCount + Amount <= 0)
	{
		OverlappedItemCount = 0;
		bShouldTraceForItems = false;
	}
	else
	{
		OverlappedItemCount += Amount;
		bShouldTraceForItems = true;
	}
}

float AShooterCharacter::GetCrosshairSpreadMultiplier() const
{
	return CrosshairSpreadMultiplier;
}

// FVector AShooterCharacter::GetCameraInterpLocation()
// {
// 	const FVector CameraWorldLocation{ FollowCamera->GetComponentLocation() };
// 	const FVector CameraForward{ FollowCamera->GetForwardVector() };
// 	// Desired = CameraWorldLocation + Forward * A + Up * B
// 	return CameraWorldLocation + CameraForward * CameraInterpDistance + FVector::UpVector * CameraInterpElevation;
// }

void AShooterCharacter::GetPickupItem(AItem* Item)
{
	Item->PlayEquipSound();

	if (const auto Weapon = Cast<AWeapon>(Item))
	{
		if (Inventory.Num() < INVENTORY_CAPACITY)
		{
			Weapon->SetSlotIndex(Inventory.Num());
			Inventory.Add(Weapon);
			Weapon->SetItemState(EItemState::EIS_PickedUp);
		}
		else
		{
			SwapWeapon(Weapon);
		}
	}
	if (const auto Ammo = Cast<AAmmo>(Item))
	{
		PickupAmmo(Ammo);
	}
}

FInterpLocation AShooterCharacter::GetInterpLocation(int32 Index)
{
	if (Index < FInterpLocations.Num())
	{
		return FInterpLocations[Index];
	}

	return FInterpLocation();
}

void AShooterCharacter::ResetPickupSoundTimer()
{
	bShouldPlayPickupSound = true;
}

void AShooterCharacter::ResetEquipSoundTimer()
{
	bShouldPlayEquipSound = true;
}

void AShooterCharacter::CharacterMove(const FInputActionInstance& Instance)
{
	if (const FVector2d MoveValue = Instance.GetValue().Get<FVector2d>(); Controller != nullptr && !MoveValue.IsZero())
	{
		const FRotator Rotator = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotator.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(ForwardDirection, MoveValue.Y);
		AddMovementInput(RightDirection, MoveValue.X);
	}
}

void AShooterCharacter::CharacterLook(const FInputActionValue& Value)
{
	const FVector2d Vector = Value.Get<FVector2d>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(Vector.X * BaseTurnRate);
		AddControllerPitchInput(Vector.Y * BaseLookUpRate);
	}
}
