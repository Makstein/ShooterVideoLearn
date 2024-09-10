// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BulletHitInterface.h"
#include "GameFramework/Character.h"
#include "Enemy.generated.h"

class USoundCue;

UCLASS()
class SHOOTERVIDEOLEARN_API AEnemy : public ACharacter, public IBulletHitInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemy();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintNativeEvent)
	void ShowHealthBar();

	UFUNCTION(BlueprintImplementableEvent)
	void HideHealthBar();

	void Die();

	void PlayHitMontage(FName Section, float PlayRate = 1.0f);

	void ResetHitReactTimer();

	UFUNCTION(BlueprintCallable)
	void StoreHitNumber(UUserWidget* HitNumber, FVector HitNumberLocation);

	UFUNCTION()
	void DestroyHitNumber(UUserWidget* HitNumber);

	void UpdateHitNumbers();

private:
	// Particle System to spawn on hit
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (AllowPrivateAccess = true))
	UParticleSystem* ImpactParticles;

	// Sound to play on hit
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (AllowPrivateAccess = true))
	USoundCue* ImpactSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (AllowPrivateAccess = true))
	float Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (AllowPrivateAccess = true))
	float MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (AllowPrivateAccess = true))
	FString HeadBone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (AllowPrivateAccess = true))
	float HealthBarDisplayTime;

	FTimerHandle HealthBarTimer;

	// Montage contains hit and death animations
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (AllowPrivateAccess = true))
	UAnimMontage* HitMontage;

	FTimerHandle HitReactTimer;

	bool bCanHitReact;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (AllowPrivateAccess = true))
	float HitReactTimeMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (AllowPrivateAccess = true))
	float HitReactTimeMax;

	// Map to store HitNumber widgets and their locations
	UPROPERTY(VisibleAnywhere, Category = "Combat", meta = (AllowPrivateAccess = true))
	TMap<UUserWidget*, FVector> HitNumbers;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (AllowPrivateAccess = true))
	float HitNumberDestroyTime;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void BulletHit_Implementation(FHitResult HitResult) override;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	                         class AController* EventInstigator, AActor* DamageCauser) override;

	FORCEINLINE FString GetHeadBone() const { return HeadBone; }

	UFUNCTION(BlueprintImplementableEvent)
	void ShowHitNumber(int32 Damage, FVector HitLocation, bool bHeadShot = false);
};
