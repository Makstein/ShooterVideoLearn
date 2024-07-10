// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "item.h"
#include "ShooterVideoLearn/AmmoType.h"
#include "Ammo.generated.h"

UCLASS()
class SHOOTERVIDEOLEARN_API AAmmo : public AItem
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAmmo();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void SetItemProperties(const EItemState State) override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Ammo, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* AmmoMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ammo, meta = (AllowPrivateAccess = "true"))
	EAmmoType AmmoType;

public:
	FORCEINLINE UStaticMeshComponent* GetAmmoMesh() const { return AmmoMesh; }
	FORCEINLINE EAmmoType GetAmmoType() const { return AmmoType; }
};
