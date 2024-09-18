#include "Explosive.h"

#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"

// Sets default values
AExplosive::AExplosive() :
	Damage(80.f)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ExplosiveMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Explosive Mesh"));
	SetRootComponent(ExplosiveMesh);

	OverlapSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Overlap Sphere"));
	OverlapSphere->SetupAttachment(GetRootComponent());
}

// Called when the game starts or when spawned
void AExplosive::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AExplosive::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AExplosive::BulletHit_Implementation(FHitResult HitResult, AActor* Shooter, AController* ShooterController)
{
	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}
	if (ExplodeParticles)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplodeParticles, HitResult.Location);
	}

	// apply explosive damage
	TArray<AActor*> OverlapActors;
	OverlapSphere->GetOverlappingActors(OverlapActors, ACharacter::StaticClass());
	for (auto Actor : OverlapActors)
	{
		UE_LOG(LogTemp, Warning, TEXT("Overlapping Actor: %s"), *Actor->GetName());

		UGameplayStatics::ApplyDamage(Actor, Damage, ShooterController, this, UDamageType::StaticClass());
	}

	Destroy();
}
