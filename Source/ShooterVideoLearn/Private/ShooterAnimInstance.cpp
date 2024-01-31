// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterAnimInstance.h"
#include "ShooterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UShooterAnimInstance::UpdateAnimationProperties(float DeltaTime)
{
	if (ShooterCharacter == nullptr) ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
	if (ShooterCharacter == nullptr) return;

	FVector Velocity = ShooterCharacter->GetVelocity();
	Velocity.Z = 0;
	Speed = Velocity.Size();

	bIsInAir = ShooterCharacter->GetCharacterMovement()->IsFalling();

	bIsAccelerating = ShooterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f;

	const FRotator AimRotation = ShooterCharacter->GetBaseAimRotation();
	const FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(ShooterCharacter->GetVelocity());
	MovementOffsetYaw = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation).Yaw;

	if (ShooterCharacter->GetVelocity().Size() > 0)
	{
		LastMovementOffsetYaw = MovementOffsetYaw;
	}

	// const FString OffsetMessage = FString::Printf(TEXT("Movement Rotation: %f"), MovementOffsetYaw);
	//
	// if (GEngine)
	// {
	// 	GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::White, OffsetMessage);
	// }

	bAiming = ShooterCharacter->GetAiming();
}

void UShooterAnimInstance::NativeInitializeAnimation()
{
	ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
}