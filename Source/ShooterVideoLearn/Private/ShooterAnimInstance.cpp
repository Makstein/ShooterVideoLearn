#include "ShooterAnimInstance.h"
#include "ShooterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

UShooterAnimInstance::UShooterAnimInstance() :
	ShooterCharacter(nullptr),
	Speed(0.f),
	bIsInAir(false),
	bIsAccelerating(false),
	MovementOffsetYaw(0.f),
	LastMovementOffsetYaw(0.f),
	bAiming(false),
	TIPCharacterYaw(0.f),
	TIPCharacterYawLastFrame(0.f),
	RootYawOffset(0.f),
	RotationCurve(0),
	RotationCurveValueLastFrame(0), Pitch(0), bReloading(false), OffsetState(EOffsetState::EOS_Hip), CharacterYaw(0),
	CharacterYawLastFrame(0), YawDelta(0)
{
}

void UShooterAnimInstance::UpdateAnimationProperties(const float DeltaTime)
{
	if (ShooterCharacter == nullptr) ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
	if (ShooterCharacter == nullptr) return;

	bReloading = ShooterCharacter->GetCombatState() == ECombatState::ECS_Reloading;

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

	bAiming = ShooterCharacter->GetAiming();

	if (bReloading)
	{
		OffsetState = EOffsetState::EOS_Reloading;
	}
	else if (bIsInAir)
	{
		OffsetState = EOffsetState::EOS_InAir;
	}
	else if (bAiming)
	{
		OffsetState = EOffsetState::EOS_Aiming;
	}
	else
	{
		OffsetState = EOffsetState::EOS_Hip;
	}

	TurnInPlace();
	Lean(DeltaTime);
}

void UShooterAnimInstance::NativeInitializeAnimation()
{
	ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
}

void UShooterAnimInstance::TurnInPlace()
{
	if (ShooterCharacter == nullptr) return;

	Pitch = ShooterCharacter->GetBaseAimRotation().Pitch;

	if (Speed > 0 || bIsInAir)
	{
		// No need to turn in place because character is moving
		RootYawOffset = 0.f;
		TIPCharacterYaw = ShooterCharacter->GetActorRotation().Yaw;
		TIPCharacterYawLastFrame = TIPCharacterYaw;
		RotationCurve = 0.f;
		RotationCurveValueLastFrame = 0.f;
	}
	else
	{
		TIPCharacterYawLastFrame = TIPCharacterYaw;
		TIPCharacterYaw = ShooterCharacter->GetActorRotation().Yaw;
		const float TIPYawDelta{TIPCharacterYaw - TIPCharacterYawLastFrame};
		RootYawOffset = UKismetMathLibrary::NormalizeAxis(RootYawOffset - TIPYawDelta);

		// 1.0 if turning, 0.0 if not
		if (const float Turning{GetCurveValue(TEXT("Turning"))}; Turning > 0)
		{
			RotationCurveValueLastFrame = RotationCurve;
			RotationCurve = GetCurveValue(TEXT("Rotation"));
			const float DeltaRotation{RotationCurve - RotationCurveValueLastFrame};

			// RootYawOffset > 0 means turning to the left
			// RootYawOffset < 0 means turning to the right
			RootYawOffset > 0 ? RootYawOffset -= DeltaRotation : RootYawOffset += DeltaRotation;
			if (const float ABSRootYawOffset{FMath::Abs(RootYawOffset)}; ABSRootYawOffset > 90.f)
			{
				const float Excess{ABSRootYawOffset - 90.f};
				RootYawOffset > 0 ? RootYawOffset -= Excess : RootYawOffset += Excess;
			}
		}

		if (GEngine) GEngine->AddOnScreenDebugMessage(1, -1, FColor::White, FString::Printf(TEXT("RootYawOffset: %f"), RootYawOffset));
	}
}

void UShooterAnimInstance::Lean(const float DeltaTime)
{
	if (ShooterCharacter == nullptr) return;
	CharacterYawLastFrame = CharacterYaw;
	CharacterYaw = ShooterCharacter->GetActorRotation().Yaw;

	const float Target{(CharacterYaw - CharacterYawLastFrame) / DeltaTime};
	const float Interp{FMath::FInterpTo(YawDelta, Target, DeltaTime, 6.f)};
	YawDelta = FMath::Clamp(Interp, -90.f, 90.f);

	if (GEngine) GEngine->AddOnScreenDebugMessage(2, -1, FColor::Cyan, FString::Printf(TEXT("YawDelta: %f"), YawDelta));
}
