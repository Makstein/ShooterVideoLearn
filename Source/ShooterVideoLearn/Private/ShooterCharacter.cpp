// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "item.h"
#include "Weapon.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"

// Sets default values
AShooterCharacter::AShooterCharacter() :
	BaseTurnRate(.5f),
	BaseLookUpRate(.5f),
	HipTurnRate(1.f),
	HipLookUpRate(1.f),
	AimingTurnRate(.2f),
	AimingLookUpRate(.2f),
	bAiming(false),
	CameraDefaultFOV(0.f),
	CameraZoomedFOV(35.f),
	ZoomInterpSpeed(20.f),
	CameraCurrentFOV(0.f),
	// 准星散布
	CrosshairSpreadMultiplier(0.f),
	CrosshairVelocityFactor(0.f),
	CrosshairInAirFactor(0.f),
	CrosshairAimFactor(0.f),
	CrosshairShootingFactor(0.f),
	// 射击参数
	ShootTimeDuration(0.05f),
	bFiringBullet(false),
	// 自动射击
	AutomaticFireRate(0.1f),
	bShouldFire(true),
	bFireButtonPressed(false),
	bShouldTraceForItems(false)
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
}

// Called every frame
void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CameraInterpZoom(DeltaTime);
	SetLookRates();
	CalculateCrosshairSpread(DeltaTime);
	TraceForItems();
}

// Called to bind functionality to input
void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);

	// Jump
	Input->BindAction(JumpInputAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
	Input->BindAction(JumpInputAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

	// Moving
	Input->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::CharacterMove);

	// Looking
	Input->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::CharacterLook);

	// Firing
	Input->BindAction(FireInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::CharacterFire);

	Input->BindAction(AimPressedAction, ETriggerEvent::Triggered, this, &AShooterCharacter::CharacterAim);

	Input->BindAction(SelectInputAction, ETriggerEvent::Triggered, this, &AShooterCharacter::CharacterSelect);
}

void AShooterCharacter::CharacterFire(const FInputActionValue& Value)
{
	if (Value.Get<float>() == 1)
	{
		bFireButtonPressed = true;
		StartFireTimer();
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
		bAiming = !bAiming;
	}

	// UE_LOG(LogTemp, Warning, TEXT("%f"), Value.Get<float>())
	// 为0时表示松开，为1时代表按下
}

void AShooterCharacter::CharacterSelect(const FInputActionValue& Value)
{
	
}

bool AShooterCharacter::GetBeamEndLocation(const FVector& MuzzleSocketLocation, FVector& OutBeamLocation)
{
	FHitResult OutHitResult;
	TraceUnderCrosshairs(OutHitResult);

	OutBeamLocation = OutHitResult.Location;

	FHitResult WeaponTraceHit;
	const FVector WeaponTraceStart = MuzzleSocketLocation;
	const FVector StartToEnd{OutBeamLocation - MuzzleSocketLocation};
	// 防止未检测到
	const FVector WeaponTraceEnd = MuzzleSocketLocation + StartToEnd * 1.25f;
	GetWorld()->LineTraceSingleByChannel(WeaponTraceHit, WeaponTraceStart, WeaponTraceEnd, ECC_Visibility);
	if (WeaponTraceHit.bBlockingHit)
	{
		OutBeamLocation = WeaponTraceHit.Location;
		return true;
	}

	return false;
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
	const FVector2D WalkSpeedRange{0.f, 600.f};
	const FVector2D VelocityRange{0.f, 1.f};
	FVector Velocity{GetVelocity()};
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
	if (bShouldFire)
	{
		FireWeapon();
		bShouldFire = false;
		GetWorldTimerManager().SetTimer(AutoFireTimer, this, &AShooterCharacter::AutoFireReset, AutomaticFireRate);
	}
}

void AShooterCharacter::FireWeapon()
{
	if (FireSound)
	{
		UGameplayStatics::PlaySound2D(this, FireSound);
	}
	if (const USkeletalMeshSocket* BarrelSocket = GetMesh()->GetSocketByName("BarrelSocket"))
	{
		const FTransform BarrelTransform = BarrelSocket->GetSocketTransform(GetMesh());

		if (MuzzleFlash)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, BarrelTransform);
		}

		// 在教程基础上更改，无论是否碰撞，都应产生子弹轨迹
		FVector BeamEnd;
		if (GetBeamEndLocation(BarrelTransform.GetLocation(), BeamEnd))
		{
			if (ImpactParticles)
			{
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, BeamEnd);
			}
		}

		if (UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(), BeamParticles, BarrelTransform))
		{
			Beam->SetVectorParameter(FName("Target"), BeamEnd);
		}
	}

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && HipFireMontage)
	{
		AnimInstance->Montage_Play(HipFireMontage);
		AnimInstance->Montage_JumpToSection(FName("StartFire"));
	}

	StartCrosshairBulletFire();
}

void AShooterCharacter::AutoFireReset()
{
	bShouldFire = true;
	if (bFireButtonPressed)
	{
		StartFireTimer();
	}
}

bool AShooterCharacter::TraceUnderCrosshairs(FHitResult& OutHitResult)
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
		const FVector Start{CrosshairWorldPosition};
		const FVector End{CrosshairWorldPosition + CrosshairWorldDirection * 50000.f};
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
		}

		return;
	}

	if (FHitResult ItemTraceResult; TraceUnderCrosshairs(ItemTraceResult))
	{
		AItem* HitItem = Cast<AItem>(ItemTraceResult.GetActor());

		if (HitItem && HitItem->GetPickupWidget())
		{
			HitItem->GetPickupWidget()->SetVisibility(true);
		}

		if (TraceHitItemLastFrame && TraceHitItemLastFrame != HitItem)
		{
			TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
		}

		TraceHitItemLastFrame = HitItem;
	}
}

AWeapon* AShooterCharacter::SpawnDefaultWeapon()
{
	if (!DefaultWeaponClass) return nullptr;

	return GetWorld()->SpawnActor<AWeapon>(DefaultWeaponClass);
}

void AShooterCharacter::EquipWeapon(AWeapon* WeaponToEquip)
{
	if (!WeaponToEquip) return;

	const USkeletalMeshSocket* HandSocket = GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (!HandSocket) return;

	HandSocket->AttachActor(WeaponToEquip, GetMesh());
	EquippedWeapon = WeaponToEquip;
	EquippedWeapon->SetItemState(EItemState::EIS_Equipped);
}

void AShooterCharacter::DropWeapon()
{
	if (!EquippedWeapon) return;

	const FDetachmentTransformRules TransformRules(EDetachmentRule::KeepWorld, true);
	EquippedWeapon->GetItemMesh()->DetachFromComponent(TransformRules);
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

float AShooterCharacter::GetCrosshairSpreadMulitplier() const
{
	return CrosshairSpreadMultiplier;
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
