// Copyright 2018 - Bernhard Rieder - All Rights Reserved.

#include "ShooterCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Weapons/ShooterWeapon.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "ChangingGuns.h"
#include "Components/HealthComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ChangingGunsPlayerState.h"
#include "Weapons/WeaponGenerator.h"

// Sets default values
AShooterCharacter::AShooterCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComp"));
	SpringArmComp->bUsePawnControlRotation = true;
	SpringArmComp->SetupAttachment(RootComponent);

	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComp"));

	GetCapsuleComponent()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);

	GetMovementComponent()->GetNavAgentPropertiesRef().bCanCrouch = true;

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(SpringArmComp);

	ZoomedFOV = 65.0;
	ZoomInterpSpeed = 20.0f;
}

FVector AShooterCharacter::GetPawnViewLocation() const
{
	if(CameraComp)
	{
		return CameraComp->GetComponentLocation();
	}
	return Super::GetPawnViewLocation();
}

// Called when the game starts or when spawned
void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	m_maxWalkSpeedDefault = GetCharacterMovement()->MaxWalkSpeed;
	m_maxWalkSpeedCrouchedDefault = GetCharacterMovement()->MaxWalkSpeedCrouched;

	DefaultFOV = CameraComp->FieldOfView;
	HealthComp->OnHealthChangedEvent.AddDynamic(this, &AShooterCharacter::onHealthChanged);

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for(const TSubclassOf<AShooterWeapon> weaponClass : StarterWeaponClasses)
	{
		AShooterWeapon* starterWeapon = GetWorld()->SpawnActor<AShooterWeapon>(weaponClass, FVector::ZeroVector, FRotator::ZeroRotator, spawnParams);
		addWeapon(starterWeapon);
	}

	if(BP_WeaponGenerator.GetDefaultObject())
	{
		m_weaponGenerator = GetWorld()->SpawnActor<AWeaponGenerator>(BP_WeaponGenerator, FVector::ZeroVector, FRotator::ZeroRotator, spawnParams);
		m_weaponGenerator->SetOwner(this);
		m_weaponGenerator->OnWeaponGenerationReadyEvent.AddDynamic(this, &AShooterCharacter::onNewWeaponGenerated);
	}
	
	//(that'd be an axis input actually)
	switchWeapon(1.0f);

}

FName AShooterCharacter::getSocketNameFor(const AShooterWeapon* weapon) const
{
	if (!weapon)
		return FName("");

	//do it like that because I don't like to fiddle around with all of those 3d models 
	switch(weapon->GetType())
	{
		case EWeaponType::Pistol: return FName("PistolSocket");
		case EWeaponType::SubMachineGun: return FName("SMGSocket");
		case EWeaponType::HeavyMachineGun: return FName("MGSocket");
		case EWeaponType::SniperRifle: return FName("SniperRifleSocket");
		case EWeaponType::Shotgun: return FName("ShotgunSocket");
		case EWeaponType::Rifle: 
		default: return FName("WeaponSocket");
	}
}


void AShooterCharacter::MoveForward(float Value)
{
	AddMovementInput(GetActorForwardVector()*Value);
}

void AShooterCharacter::MoveRight(float Value)
{
	AddMovementInput(GetActorRightVector()*Value);
}

void AShooterCharacter::BeginCrouch()
{
	Crouch();
}

void AShooterCharacter::EndCrouch()
{
	UnCrouch();
}

void AShooterCharacter::BeginZoom()
{
	bWantsToZoom = true;
}

void AShooterCharacter::EndZoom()
{
	bWantsToZoom = false;
}

void AShooterCharacter::beginRun()
{
	if(auto movementComp = GetCharacterMovement())
	{
		movementComp->MaxWalkSpeed *= 1.5f;
	}
}

void AShooterCharacter::endRun()
{
	if (auto movementComp = GetCharacterMovement())
	{
		movementComp->MaxWalkSpeed /= 1.5f;
	}
}

void AShooterCharacter::reloadWeapon()
{
	if(m_equippedWeapon)
	{
		m_equippedWeapon->StartMagazineReloading();
	}
}

void AShooterCharacter::equipWeapon(AShooterWeapon* weapon)
{
	if (weapon)
	{
		if(m_equippedWeapon && m_lastEquippedWeapon != m_equippedWeapon)
		{
			m_lastEquippedWeapon = m_equippedWeapon;
		}
		m_equippedWeapon = weapon;
		m_equippedWeapon->Equip(this);
		m_equippedWeapon->SetActorHiddenInGame(false);
		m_equippedWeapon->SetActorEnableCollision(true);
		OnCurrentWeaponChangedEvent.Broadcast(m_equippedWeapon);
		GetCharacterMovement()->MaxWalkSpeed = m_maxWalkSpeedDefault * weapon->GetWalkinSpeedModifier();
		GetCharacterMovement()->MaxWalkSpeedCrouched = m_maxWalkSpeedCrouchedDefault * weapon->GetWalkinSpeedModifier();
	}
}

void AShooterCharacter::disarmWeapon(AShooterWeapon* weapon)
{
	if (weapon)
	{
		weapon->Disarm();
		weapon->SetActorHiddenInGame(true);
		weapon->SetActorEnableCollision(false);
	}
}

void AShooterCharacter::switchWeapon(float val)
{
	if (!FMath::IsNearlyZero(val) && m_availableWeapons.Num() > 0)
	{
		int32 nextIdx = 0;
		if (m_equippedWeapon)
		{
			bool nextWeapon = val >= 0.f;
			int32 idx = m_availableWeapons.Find(m_equippedWeapon);
			if(nextWeapon)
			{
				++idx;
				nextIdx = idx >= m_availableWeapons.Num() ? 0 : idx;
			}
			else
			{
				--idx;
				nextIdx = idx < 0 ? m_availableWeapons.Num() - 1 : idx;
			}
			if(m_equippedWeapon == m_availableWeapons[nextIdx])
			{
				//do nothing!
				return;
			}
			disarmWeapon(m_equippedWeapon);
		}
		equipWeapon(m_availableWeapons[nextIdx]);
	}
}

void AShooterCharacter::switchToLastEquipedWeapon()
{
	if(m_lastEquippedWeapon && m_lastEquippedWeapon != m_equippedWeapon)
	{
		disarmWeapon(m_equippedWeapon);
		equipWeapon(m_lastEquippedWeapon);
	}
}

void AShooterCharacter::StartFire()
{
	if(m_equippedWeapon)
	{
		m_equippedWeapon->StartFire();
	}
}

void AShooterCharacter::StopFire()
{
	if (m_equippedWeapon)
	{
		m_equippedWeapon->StopFire();
	}
}

bool AShooterCharacter::IsMoving() const
{
	return GetCharacterMovement()->GetCurrentAcceleration() != FVector::ZeroVector;
}

bool AShooterCharacter::IsCrouching() const
{
	return GetCharacterMovement()->IsCrouching();
}

void AShooterCharacter::addWeapon(AShooterWeapon* weapon)
{
	if(weapon)
	{
		weapon->SetOwner(this);
		weapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, getSocketNameFor(weapon));
		disarmWeapon(weapon);
		m_availableWeapons.Add(weapon);
	}

}

void AShooterCharacter::removeWeapon(AShooterWeapon* weapon)
{
	if(weapon && weapon != m_equippedWeapon)
	{
		m_availableWeapons.Remove(weapon);
		if(!weapon->IsPendingKill())
		{
			weapon->Destroy();
		}
	}
}

void AShooterCharacter::dismantleEquippedWeaponAndGenerateNew()
{
	if (m_weaponGenerator->IsGenerating() || !m_weaponGenerator->IsReadyToUse())
		return;

	OnStartedWeaponGeneratorEvent.Broadcast();

	AShooterWeapon* dismantle = m_equippedWeapon;
	switchWeapon(1.f);
	if (AChangingGunsPlayerState* state = Cast<AChangingGunsPlayerState>(PlayerState))
	{
		state->RemoveWeaponFromStatistics(dismantle);
	}
	removeWeapon(dismantle);
	dismantle = nullptr;

	m_weaponGenerator->DismantleWeapon(m_equippedWeapon);
}

void AShooterCharacter::onNewWeaponGenerated(AShooterWeapon* weapon)
{
	if (!weapon)
		return;

	OnNewGeneratedWeaponAvailableEvent.Broadcast();

	addWeapon(weapon);
	equipWeapon(weapon);
}

void AShooterCharacter::onHealthChanged(const UHealthComponent* HealthComponent, float Health, float HealthDelta, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if(Health <= 0.f && !bDied)
	{
		bDied = true;

		StopFire();
		GetMovementComponent()->StopMovementImmediately();
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		DetachFromControllerPendingDestroy();
		SetLifeSpan(10.f);

		for(AShooterWeapon* weapon : m_availableWeapons)
		{
			weapon->SetLifeSpan(10.f);
		}
	}
}

// Called every frame
void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	float targetFOV = bWantsToZoom ? ZoomedFOV : DefaultFOV;
	float newFOV = FMath::FInterpTo(CameraComp->FieldOfView, targetFOV, DeltaTime, ZoomInterpSpeed);
	CameraComp->SetFieldOfView(newFOV);
}

// Called to bind functionality to input
void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("LookUp", this, &AShooterCharacter::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("Turn", this, &AShooterCharacter::AddControllerYawInput);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &AShooterCharacter::BeginCrouch);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &AShooterCharacter::EndCrouch);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);

	PlayerInputComponent->BindAction("Zoom", IE_Pressed, this, &AShooterCharacter::BeginZoom);
	PlayerInputComponent->BindAction("Zoom", IE_Released, this, &AShooterCharacter::EndZoom);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AShooterCharacter::StartFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AShooterCharacter::StopFire);

	PlayerInputComponent->BindAction("Run", IE_Pressed, this, &AShooterCharacter::beginRun);
	PlayerInputComponent->BindAction("Run", IE_Released, this, &AShooterCharacter::endRun);

	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &AShooterCharacter::reloadWeapon);

	PlayerInputComponent->BindAction("SwitchToLastEquippedWeapon", IE_Pressed, this, &AShooterCharacter::switchToLastEquipedWeapon);
	PlayerInputComponent->BindAxis("SwitchWeapon", this, &AShooterCharacter::switchWeapon);
	PlayerInputComponent->BindAction("DismantleEquippedWeaponAndGenerateNew", IE_Pressed, this, &AShooterCharacter::dismantleEquippedWeaponAndGenerateNew);
}

