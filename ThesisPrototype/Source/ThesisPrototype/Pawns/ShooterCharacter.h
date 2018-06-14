// Copyright 2018 - Bernhard Rieder - All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ShooterCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentWeaponChangedEvent, class AShooterWeapon*, newCurrentWeapon);



class UCameraComponent;
class USpringArmComponent;
class AShooterWeapon;
class UHealthComponent;
class UDamageType;
class AController;
class AWeaponGenerator;
class AChangingGunsPlayerState;

UENUM(BlueprintType)
enum class EActorType : uint8
{
	Player,
	Bot,
	BotBoss
};

UCLASS()
class THESISPROTOTYPE_API AShooterCharacter : public ACharacter
{
	GENERATED_BODY()

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* SpringArmComp;

	UPROPERTY(EditDefaultsOnly, Category = "Player")
	float ZoomedFOV;

	UPROPERTY(EditDefaultsOnly, Category = "Player", meta = (ClampMin = 0.1, ClampMax = 100.0))
	float ZoomInterpSpeed;

	UPROPERTY(EditDefaultsOnly, Category = "Player")
	TArray<TSubclassOf<AShooterWeapon>> StarterWeaponClasses;

	UPROPERTY(EditDefaultsOnly, Category = "Player")
	TSubclassOf<AWeaponGenerator> BP_WeaponGenerator;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Player")
	UHealthComponent* HealthComp;

	UPROPERTY(BlueprintReadOnly, Category = "Player")
	bool bDied;

	UPROPERTY(BlueprintAssignable, Category = "Player")
	FOnCurrentWeaponChangedEvent OnCurrentWeaponChangedEvent;

	UPROPERTY(BlueprintReadOnly, Category = "Player|Weapon", meta = (DisplayName = "Equipped Weapon"))
	AShooterWeapon* m_equippedWeapon;


public:
	// Sets default values for this character's properties
	AShooterCharacter();

	virtual FVector GetPawnViewLocation() const override;

	UFUNCTION(BlueprintCallable, Category = "Player")
	void StartFire();
	UFUNCTION(BlueprintCallable, Category = "Player")
	void StopFire();

	UFUNCTION(BlueprintCallable, Category="Player")
	bool IsMoving() const;
	UFUNCTION(BlueprintCallable, Category = "Player")
	bool IsCrouching() const;
	UFUNCTION(BlueprintCallable, Category = "Player")
	bool IsAiming() const { return bWantsToZoom; }

	FORCEINLINE AShooterWeapon* GetEquippedWeapon() const { return m_equippedWeapon; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void BeginCrouch();
	void EndCrouch();
	void BeginZoom();
	void EndZoom();
	void beginRun();
	void endRun();
	void reloadWeapon();
	void addWeapon(AShooterWeapon* weapon);
	void equipWeapon(AShooterWeapon* weapon);
	void disarmWeapon(AShooterWeapon* weapon);
	//switches to next weapon if val is positive otherwise to the previous weapon
	void switchWeapon(float val);
	void switchToLastEquipedWeapon();
	void removeWeapon(AShooterWeapon* weapon);
	void dismantleEquippedWeaponAndGenerateNew();

	//this is the callback for the weapon generator
	UFUNCTION()
	void onNewWeaponGenerated(AShooterWeapon* weapon);

	UFUNCTION()
	void onHealthChanged(const UHealthComponent* HealthComponent, float Health, float HealthDelta, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	FName getSocketNameFor(const AShooterWeapon* weapon) const;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	bool bWantsToZoom;
	float DefaultFOV;

	AShooterWeapon* m_lastEquippedWeapon;
	TArray<AShooterWeapon*> m_availableWeapons;
	AWeaponGenerator* m_weaponGenerator;

	float m_maxWalkSpeedDefault;
	float m_maxWalkSpeedCrouchedDefault;
};
