// Copyright 2021 - Samuel Hubbard


#include "Ammo.h"

#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"

AAmmo::AAmmo()
{
	AmmoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AmmoMesh"));
	SetRootComponent(AmmoMesh);

	GetCollisionBox()->SetupAttachment(GetRootComponent());
	GetPickupWidget()->SetupAttachment(GetRootComponent());
	GetAreaSphere()->SetupAttachment(GetRootComponent());
}

void AAmmo::BeginPlay()
{
	Super::BeginPlay();
}

void AAmmo::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}