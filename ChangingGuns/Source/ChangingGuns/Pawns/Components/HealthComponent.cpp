// Copyright 2018 - Bernhard Rieder - All Rights Reserved.

#include "HealthComponent.h"
#include "GameFramework/Actor.h"

UHealthComponent::UHealthComponent()
{
	DefaultHealth = 100;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	if(AActor* owner = GetOwner())
	{
		owner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::handleTakeAnyDamage);
	}
	Health = DefaultHealth;
}

void UHealthComponent::handleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if(Damage <= 0.0f)
	{
		return;
	}

	Health = FMath::Clamp(Health - Damage, 0.f, DefaultHealth);

	UE_LOG(LogTemp, Log, TEXT("Health Changed: %s"), *FString::SanitizeFloat(Health));
}
