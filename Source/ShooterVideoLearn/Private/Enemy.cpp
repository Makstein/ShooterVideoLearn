// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy.h"

#include "EnemyAIController.h"
#include "ShooterCharacter.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Sound/SoundCue.h"

// Sets default values
AEnemy::AEnemy() :
	Health(100.f),
	MaxHealth(100.f),
	HealthBarDisplayTime(4.f),
	bCanHitReact(true),
	HitReactTimeMin(.5f),
	HitReactTimeMax(3.f),
	HitNumberDestroyTime(1.5f),
	bStunned(false),
	StunChance(.5f),
	AttackLFast(TEXT("AttackLFast")),
	AttackRFast(TEXT("AttackRFast")),
	AttackL(TEXT("AttackL")),
	AttackR(TEXT("AttackR")),
	BaseDamage(20.f),
	LeftWeaponSocket(TEXT("FX_Trail_L_01")),
	RightWeaponSocket(TEXT("FX_Trail_R_01")),
	bCanAttack(true),
	AttackWaitTime(1.f)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AgroSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Agro Sphere"));
	AgroSphere->SetupAttachment(GetRootComponent());

	CombatRangeSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Combat Range Sphere"));
	CombatRangeSphere->SetupAttachment(GetRootComponent());

	LeftWeaponCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("Left Weapon Box"));
	LeftWeaponCollision->SetupAttachment(GetMesh(), FName("LeftWeaponBone"));
	RightWeaponCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("Right Weapon Box"));
	RightWeaponCollision->SetupAttachment(GetMesh(), FName("RightWeaponBone"));
}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	AgroSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::AEnemy::AgroSphereOverlap);

	CombatRangeSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::CombatRangeOverlap);
	CombatRangeSphere->OnComponentEndOverlap.AddDynamic(this, &AEnemy::CombatRangeEndOverlap);

	LeftWeaponCollision->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::OnLeftWeaponOverlap);
	RightWeaponCollision->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::OnRightWeaponOverlap);

	LeftWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftWeaponCollision->SetCollisionObjectType(ECC_WorldDynamic);
	LeftWeaponCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	LeftWeaponCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RightWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightWeaponCollision->SetCollisionObjectType(ECC_WorldDynamic);
	RightWeaponCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	RightWeaponCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Ignore the camera for mesh and capsule, to make the view normal when enemy attacking the player
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	EnemyAIController = Cast<AEnemyAIController>(GetController());

	if (EnemyAIController)
	{
		EnemyAIController->GetBlackboardComponent()->SetValueAsBool(TEXT("CanAttack"), true);
	}
	
	const FVector WorldPatrolPoint = UKismetMathLibrary::TransformLocation(GetActorTransform(), PatrolPoint);
	const FVector WorldPatrolPoint2 = UKismetMathLibrary::TransformLocation(GetActorTransform(), PatrolPoint2);
	DrawDebugSphere(GetWorld(), WorldPatrolPoint, 25.f, 12, FColor::Red, true);
	DrawDebugSphere(GetWorld(), WorldPatrolPoint2, 25.f, 12, FColor::Red, true);
	if (EnemyAIController != nullptr)
	{
		EnemyAIController->GetBlackboardComponent()->SetValueAsVector(TEXT("PatrolPoint"), WorldPatrolPoint);
		EnemyAIController->GetBlackboardComponent()->SetValueAsVector(TEXT("PatrolPoint2"), WorldPatrolPoint2);
		EnemyAIController->RunBehaviorTree(BehaviorTree);
	}
}

void AEnemy::Die()
{
	HideHealthBar();
}

void AEnemy::PlayHitMontage(FName Section, float PlayRate)
{
	if (!bCanHitReact) return;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && HitMontage)
	{
		AnimInstance->Montage_Play(HitMontage, PlayRate);
		AnimInstance->Montage_JumpToSection(Section, HitMontage);
		bCanHitReact = false;
		const float HitReactTime{ FMath::FRandRange(HitReactTimeMin, HitReactTimeMax) };
		GetWorldTimerManager().SetTimer(HitReactTimer, this, &AEnemy::ResetHitReactTimer, HitReactTime);
	}
}

void AEnemy::PlayAttackMontage(FName Section, float PlayRate)
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && AttackMontage)
	{
		AnimInstance->Montage_Play(AttackMontage, PlayRate);
		AnimInstance->Montage_JumpToSection(Section, AttackMontage);
	}
	bCanAttack = false;
	GetWorldTimerManager().SetTimer(AttackWaitTimer, this, &AEnemy::ResetCanAttack, AttackWaitTime);
	if (EnemyAIController)
	{
		EnemyAIController->GetBlackboardComponent()->SetValueAsBool(TEXT("CanAttack"), bCanAttack);
	}
}

void AEnemy::ResetHitReactTimer()
{
	bCanHitReact = true;
}

void AEnemy::StoreHitNumber(UUserWidget* HitNumber, FVector HitNumberLocation)
{
	HitNumbers.Add(HitNumber, HitNumberLocation);

	FTimerHandle HitNumberTimer;
	FTimerDelegate HitNumberDelegate;
	HitNumberDelegate.BindUFunction(this, FName("DestroyHitNumber"), HitNumber);
	GetWorld()->GetTimerManager().SetTimer(HitNumberTimer, HitNumberDelegate, HitNumberDestroyTime, false);
}

void AEnemy::DestroyHitNumber(UUserWidget* HitNumber)
{
	HitNumbers.Remove(HitNumber);
	HitNumber->RemoveFromParent();
}

void AEnemy::UpdateHitNumbers()
{
	for (auto& HitPair : HitNumbers)
	{
		UUserWidget* HitNumber{ HitPair.Key };
		FVector Location{ HitPair.Value };
		FVector2D ScreenPosition;
		UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), Location, ScreenPosition);
		HitNumber->SetPositionInViewport(ScreenPosition);
	}
}

void AEnemy::AgroSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                               int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor == nullptr) return;

	if (const auto Character = Cast<AShooterCharacter>(OtherActor))
	{
		EnemyAIController->GetBlackboardComponent()->SetValueAsObject(TEXT("Target"), Character);
	}
}

void AEnemy::SetStunned(bool Stunned)
{
	bStunned = Stunned;

	if (EnemyAIController)
	{
		EnemyAIController->GetBlackboardComponent()->SetValueAsBool(TEXT("Stunned"), bStunned);
	}
}

void AEnemy::CombatRangeOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                const FHitResult& SweepResult)
{
	if (OtherActor == nullptr) return;
	if (Cast<AShooterCharacter>(OtherActor))
	{
		bInAttackRange = true;
		if (EnemyAIController)
		{
			EnemyAIController->GetBlackboardComponent()->SetValueAsBool(TEXT("InAttackRange"), bInAttackRange);
		}
	}
}

void AEnemy::CombatRangeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor == nullptr) return;
	if (Cast<AShooterCharacter>(OtherActor))
	{
		bInAttackRange = false;
		if (EnemyAIController)
		{
			EnemyAIController->GetBlackboardComponent()->SetValueAsBool(TEXT("InAttackRange"), bInAttackRange);
		}
	}
}

FName AEnemy::GetAttackSectionName()
{
	FName SectionName;
	switch (FMath::RandRange(1, 4))
	{
	case 1:
		SectionName = AttackLFast;
		break;
	case 2:
		SectionName = AttackRFast;
		break;
	case 3:
		SectionName = AttackL;
		break;
	case 4:
		SectionName = AttackR;
	default: ;
	}
	return SectionName;
}

void AEnemy::OnLeftWeaponOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                 const FHitResult& SweepResult)
{
	if (const auto Character = Cast<AShooterCharacter>(OtherActor))
	{
		DoDamage(Character);
		SpawnBlood(Character, LeftWeaponSocket);
		StunCharacter(Character);
	}
}

void AEnemy::OnRightWeaponOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                  const FHitResult& SweepResult)
{
	if (const auto Character = Cast<AShooterCharacter>(OtherActor))
	{
		DoDamage(Character);
		SpawnBlood(Character, RightWeaponSocket);
		StunCharacter(Character);
	}
}

void AEnemy::ActivateLeftWeapon()
{
	LeftWeaponCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AEnemy::DeactivateLeftWeapon()
{
	LeftWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEnemy::ActivateRightWeapon()
{
	RightWeaponCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AEnemy::DeactivateRightWeapon()
{
	RightWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEnemy::DoDamage(AShooterCharacter* Victim)
{
	if (Victim == nullptr) return;
	UGameplayStatics::ApplyDamage(Victim, BaseDamage, EnemyAIController, this, UDamageType::StaticClass());
	if (Victim->GetMeleeImpactSound())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Victim->GetMeleeImpactSound(), GetActorLocation());
	}
}

void AEnemy::SpawnBlood(const AShooterCharacter* Victim, const FName SocketName)
{
	if (const USkeletalMeshSocket* TipSocket{ GetMesh()->GetSocketByName(SocketName) })
	{
		const FTransform SocketTransform{ TipSocket->GetSocketTransform(GetMesh()) };
		if (Victim->GetBloodParticles())
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Victim->GetBloodParticles(), SocketTransform);
		}
	}
}

void AEnemy::StunCharacter(AShooterCharacter* Victim)
{
	if (Victim == nullptr) return;
	if (const float Stun{ FMath::FRandRange(0.f, 1.f) }; Stun <= Victim->GetStunChance())
	{
		Victim->Stun();
	}
}

void AEnemy::ResetCanAttack()
{
	bCanAttack = true;
	if (EnemyAIController)
	{
		EnemyAIController->GetBlackboardComponent()->SetValueAsBool(TEXT("CanAttack"), bCanAttack);
	}
}

void AEnemy::ShowHealthBar_Implementation()
{
	GetWorldTimerManager().ClearTimer(HealthBarTimer);
	GetWorldTimerManager().SetTimer(HealthBarTimer, this, &AEnemy::HideHealthBar, HealthBarDisplayTime);
}

// Called every frame
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateHitNumbers();
}

// Called to bind functionality to input
void AEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AEnemy::BulletHit_Implementation(FHitResult HitResult)
{
	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}
	if (ImpactParticles)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, HitResult.Location);
	}
	ShowHealthBar();

	// Determine whether this hit stuns
	if (const float Stunned = FMath::FRandRange(0.f, 1.f); Stunned <= StunChance)
	{
		PlayHitMontage(FName("HitReactionFront"));
		SetStunned(true);
	}
}

float AEnemy::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator,
                         AActor* DamageCauser)
{
	// set blackboard enemy target to make enemy start chasing the character
	if (EnemyAIController)
	{
		EnemyAIController->GetBlackboardComponent()->SetValueAsObject(FName("Target"), DamageCauser);
	}
	
	if (Health - DamageAmount <= 0.f)
	{
		Health = 0.f;
		Die();
	}
	else
	{
		Health -= DamageAmount;
	}
	return DamageAmount;
}
