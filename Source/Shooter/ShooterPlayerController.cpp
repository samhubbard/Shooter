// Copyright 2021 - Samuel Hubbard


#include "ShooterPlayerController.h"

#include "Blueprint/UserWidget.h"

AShooterPlayerController::AShooterPlayerController()
{
	
}

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (HUDOverlayClass)
	{
		HUDOverlay = CreateWidget<UUserWidget>(this, HUDOverlayClass);
		if (HUDOverlay)
		{
			HUDOverlay->AddToViewport();
			HUDOverlay->SetVisibility(ESlateVisibility::Visible);
		}
	}
}
