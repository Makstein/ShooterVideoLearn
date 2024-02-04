// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon.h"

AWeapon::AWeapon() :
	ThrowWeaponTime(0.7f),
	bFalling(false)
{
}

void AWeapon::ThrowWeapon()
{
	const FRotator MeshRotation{0.f, GetItemMesh()->GetComponentRotation().Yaw, 0.f};
	GetItemMesh()->SetWorldRotation(MeshRotation, false, nullptr, ETeleportType::TeleportPhysics);

	const FVector MeshForward{GetItemMesh()->GetForwardVector()};
	const FVector MeshRight{GetItemMesh()->GetRightVector()};
	// Direction of throw
	FVector ImpulseDirection = MeshRight.RotateAngleAxis(-20.f, MeshForward);

	const float RandomRotation{FMath::FRandRange(0, 45.f)};
	ImpulseDirection = ImpulseDirection.RotateAngleAxis(RandomRotation, FVector::UpVector);
	ImpulseDirection *= 20'0000.f;
	GetItemMesh()->AddImpulse(ImpulseDirection);

	bFalling = true;
	GetWorldTimerManager().SetTimer(ThrowWeaponTimer, this, &AWeapon::StopFalling, ThrowWeaponTime);
}

void AWeapon::StopFalling()
{
	bFalling = false;
}
