// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon.h"

AWeapon::AWeapon() :
	ThrowWeaponTime(0.7f),
	bFalling(false),
	Ammo(30),
	MagazineCapacity(30),
	WeaponType(EWeaponType::EWT_SubMachineGun),
	AmmoType(EAmmoType::EAT_9MM),
	ReloadMontageSection(FName(TEXT("Reload SMG"))), bMovingClip(false),
	ClipBoneName(TEXT("smg_clip")),
	SlideDisplacement(0.f),
	SlideDisplacementTime(.2f),
	bMovingSlide(false),
	MaxSlideDisplacement(4.f),
	MaxRecoilRotation(20.f),
	bAutomatic(true)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// keep weapon upright
	if (GetItemState() == EItemState::EIS_Falling && bFalling)
	{
		const FRotator MeshRotation{0.f, GetItemMesh()->GetComponentRotation().Yaw, 0.f};
		GetItemMesh()->SetWorldRotation(MeshRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	UpdateSlideDisplacement();
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
	ImpulseDirection *= 10'000.f;
	GetItemMesh()->AddImpulse(ImpulseDirection);

	bFalling = true;
	GetWorldTimerManager().SetTimer(ThrowWeaponTimer, this, &AWeapon::StopFalling, ThrowWeaponTime);

	EnableGlowMaterial();
}

void AWeapon::DecrementAmmo()
{
	Ammo--;
	if (Ammo <= 0)
	{
		Ammo = 0;
	}
}

void AWeapon::StartSlideTimer()
{
	bMovingSlide = true;
	GetWorldTimerManager().SetTimer(SlideTimer, this, &AWeapon::FinishMovingSlide, SlideDisplacementTime);
}

void AWeapon::ReloadAmmo(int32 Amount)
{
	checkf(Ammo + Amount <= MagazineCapacity, TEXT("Attempted to reload with more than magazine capacity!"));
	Ammo += Amount;
}

bool AWeapon::ClipIsFull() const
{
	return Ammo >= MagazineCapacity;
}

void AWeapon::StopFalling()
{
	bFalling = false;
	SetItemState(EItemState::EIS_Pickup);

	StartPulseTimer();
}

void AWeapon::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const FString WeaponTablePath{
		TEXT("/Script/Engine.DataTable'/Game/_Game/DataTables/WeaponDataTable.WeaponDataTable'")
	};

	if (const UDataTable* WeaponTableObject = Cast<UDataTable>(
		StaticLoadObject(UDataTable::StaticClass(), nullptr, *WeaponTablePath)))
	{
		const FWeaponDataTable* WeaponTableRow = nullptr;
		switch (WeaponType)
		{
		case EWeaponType::EWT_SubMachineGun:
			WeaponTableRow = WeaponTableObject->FindRow<FWeaponDataTable>(FName("SubmachineGun"), TEXT(""));
			break;
		case EWeaponType::EWT_AssaultRifle:
			WeaponTableRow = WeaponTableObject->FindRow<FWeaponDataTable>(FName("AssaultRifle"), TEXT(""));
			break;
		case EWeaponType::EWT_Pistol:
			WeaponTableRow = WeaponTableObject->FindRow<FWeaponDataTable>(FName("Pistol"), TEXT(""));
			break;
		default:
			break;
		}

		if (WeaponTableRow)
		{
			AmmoType = WeaponTableRow->AmmoType;
			Ammo = WeaponTableRow->WeaponAmmo;
			MagazineCapacity = WeaponTableRow->MagazineCapacity;
			SetPickupSound(WeaponTableRow->PickupSound);
			SetEquipSound(WeaponTableRow->EquipSound);
			GetItemMesh()->SetSkeletalMesh(WeaponTableRow->ItemMesh);
			SetItemName(WeaponTableRow->ItemName);
			SetIconItem(WeaponTableRow->InventoryIcon);
			SetIconAmmo(WeaponTableRow->AmmoIcon);

			SetMaterialInstance(WeaponTableRow->MaterialInstance);
			PreviousMaterialIndex = GetMaterialIndex();
			GetItemMesh()->SetMaterial(PreviousMaterialIndex, nullptr);
			SetMaterialIndex(WeaponTableRow->MaterialIndex);
			SetClipBoneName(WeaponTableRow->ClipBoneName);
			SetReloadMontageSection(WeaponTableRow->ReloadMontageSection);
			GetItemMesh()->SetAnimInstanceClass(WeaponTableRow->AnimBP);
			AutoFireRate = WeaponTableRow->AutoFireRate;
			MuzzleFlash = WeaponTableRow->MuzzleFlash;
			FireSound = WeaponTableRow->FireSound;
			BoneToHide = WeaponTableRow->BoneToHide;
			bAutomatic = WeaponTableRow->bAutomatic;

			CrosshairMiddle = WeaponTableRow->CrosshairMiddle;
			CrosshairLeft = WeaponTableRow->CrosshairLeft;
			CrosshairRight = WeaponTableRow->CrosshairRight;
			CrosshairBottom = WeaponTableRow->CrosshairBottom;
			CrosshairTop = WeaponTableRow->CrosshairTop;
		}

		if (GetMaterialInstance())
		{
			SetDynamicMaterialInstance(UMaterialInstanceDynamic::Create(GetMaterialInstance(), this));
			GetDynamicMaterialInstance()->SetVectorParameterValue(TEXT("FresnelColor"), GetGlowColor());
			GetItemMesh()->SetMaterial(GetMaterialIndex(), GetDynamicMaterialInstance());
			EnableGlowMaterial();
		}
	}
}

void AWeapon::BeginPlay()
{
	Super::BeginPlay();
	if (BoneToHide != FName(""))
	{
		GetItemMesh()->HideBoneByName(BoneToHide, EPhysBodyOp::PBO_None);
	}
}

void AWeapon::FinishMovingSlide()
{
	bMovingSlide = false;
}

void AWeapon::UpdateSlideDisplacement()
{
	if (!SlideDisplacementCurve) return;

	const float ElapsedTime{ GetWorldTimerManager().GetTimerElapsed(SlideTimer) };
	const float CurveValue{ SlideDisplacementCurve->GetFloatValue(ElapsedTime) };
	SlideDisplacement = CurveValue * MaxSlideDisplacement;
	RecoilRotation = CurveValue * MaxRecoilRotation;
}
