// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "item.h"
#include "Weapon.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTERVIDEOLEARN_API AWeapon : public AItem
{
	GENERATED_BODY()

public:
	AWeapon();

	virtual void Tick(float DeltaSeconds) override;
protected:
	void StopFalling();
private:
	FTimerHandle ThrowWeaponTimer;
	float ThrowWeaponTime;
	bool bFalling;
public:
	// Add an impulse to the Weapon
	void ThrowWeapon();

};
