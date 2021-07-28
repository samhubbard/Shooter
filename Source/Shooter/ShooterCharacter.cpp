// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterCharacter.h"

#include "Item.h"
#include "Weapon.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/WidgetComponent.h"
#include "Particles/ParticleSystemComponent.h"

AShooterCharacter::AShooterCharacter() :
	BaseTurnRate(45.f),
	BaseLookUpRate(45.f),
	HipTurnRate(90.f),
	HipLookUpRate(90.f),
	AimingTurnRate(20.f),
	AimingLookUpRate(20.f),
	MouseHipTurnRate(1.f),
	MouseHipLookUpRate(1.f),
	MouseAimingTurnRate(0.2f),
	MouseAimingLookUpRate(0.2f),
	bAiming(false),
	CameraDefaultFOV(0.f),
	CameraZoomedFOV(35.f),
	CameraCurrentFOV(0.f),
	ZoomInterpSpeed(20.f),
	CrosshairSpreadMultiplier(0.f),
	CrosshairVelocityFactor(0.f),
	CrosshairInAirFactor(0.f),
	CrosshairAimFactor(0.f),
	CrosshairShootingFactor(0.f),
	ShootTimeDuration(0.05f),
	bFiringBullet(false),
	bFireButtonPressed(false),
	bShouldFire(true),
	AutomaticFireRate(0.1f),
	bShouldTraceForItems(false),
	CameraInterpDistance(250.f),
	CameraInterpElevation(65.f),
	Starting9mmAmmo(85),
	StartingARAmmo(120),
	CombatState(ECombatState::ECS_Unoccupied)
{
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Camera Boom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 180.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 70.f);

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Follow Camera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	HandSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("HandSceneComp"));
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (FollowCamera)
	{
		CameraDefaultFOV = GetFollowCamera()->FieldOfView;
		CameraCurrentFOV = CameraDefaultFOV;
	}

	EquipWeapon(SpawnDefaultWeapon());

	InitializeAmmoMap();
}

void AShooterCharacter::MoveForward(float Value)
{
	if (Controller != nullptr && Value != 0.f)
	{
		const FRotator Rotation{ Controller->GetControlRotation() };
		const FRotator YawRotation{ 0, Rotation.Yaw, 0 };

		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::X) };

		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::MoveRight(float Value)
{
	if ((Controller != nullptr) && (Value != 0.f))
	{
		const FRotator Rotation{ Controller->GetControlRotation() };
		const FRotator YawRotation{ 0, Rotation.Yaw, 0 };

		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::Y) };

		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::TurnAtRate(float Rate)
{
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AShooterCharacter::LookUpAtRate(float Rate)
{
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AShooterCharacter::Turn(float Value)
{
	float TurnScaleFactor { };
	
	if (bAiming)
		TurnScaleFactor = MouseAimingTurnRate;
	else
		TurnScaleFactor = MouseHipTurnRate;

	AddControllerYawInput(Value * TurnScaleFactor);
}

void AShooterCharacter::LookUp(float Value)
{
	float LookUpScaleFactor { };

	if (bAiming)
		LookUpScaleFactor = MouseAimingLookUpRate;
	else
		LookUpScaleFactor = MouseHipLookUpRate;

	AddControllerPitchInput(Value * LookUpScaleFactor);
}

void AShooterCharacter::FireWeapon()
{
	if (EquippedWeapon == nullptr)
		return;

	if (CombatState != ECombatState::ECS_Unoccupied)
		return;

	if (WeaponHasAmmo())
	{
		PlayFireSound();
		SendBullet();
		PlayGunFireMontage();
		StartCrosshairBulletFire();
		EquippedWeapon->DecrementAmmo();

		StartFireTimer();
	}
}

void AShooterCharacter::PlayFireSound()
{
	if (FireSound)
		UGameplayStatics::PlaySound2D(this, FireSound);
}

void AShooterCharacter::SendBullet()
{
	const USkeletalMeshSocket* BarrelSocket = EquippedWeapon->GetItemMesh()->GetSocketByName("BarrelSocket");
	if (BarrelSocket)
	{
		const FTransform SocketTransform = BarrelSocket->GetSocketTransform(EquippedWeapon->GetItemMesh());

		if (MuzzleFlash)
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);

		FVector BeamEnd;
		bool bBeamEnd = GetBeamEndLocation(SocketTransform.GetLocation(), BeamEnd);

		if (bBeamEnd)
		{
			// spawn impact particles after updating beam end point
			if (ImpactParticles)
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, BeamEnd);

			if (BeamParticles)
			{
				UParticleSystemComponent* Beam =
					UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BeamParticles, SocketTransform);

				if (Beam)
					Beam->SetVectorParameter(FName("Target"), BeamEnd);
			}
		}
	}
}

void AShooterCharacter::PlayGunFireMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HipFireMontage)
	{
		AnimInstance->Montage_Play(HipFireMontage);
		AnimInstance->Montage_JumpToSection(FName("StartFire"));
	}
}

void AShooterCharacter::ReloadButtonPressed()
{
	ReloadWeapon();
}

void AShooterCharacter::ReloadWeapon()
{
	if (CombatState != ECombatState::ECS_Unoccupied)
		return;
	if (EquippedWeapon == nullptr)
		return;

	if (CarryingAmmo() && !EquippedWeapon->ClipIsFull())
	{
		CombatState = ECombatState::ECS_Reloading;
	
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (ReloadMontage && AnimInstance)
		{
			AnimInstance->Montage_Play(ReloadMontage);
			AnimInstance->Montage_JumpToSection(EquippedWeapon->GetReloadMontageSection());
		}
	}
}

void AShooterCharacter::FinishReloading()
{
	CombatState = ECombatState::ECS_Unoccupied;

	if (EquippedWeapon == nullptr) return;
	
	const auto AmmoType { EquippedWeapon->GetAmmoType() };
	if (AmmoMap.Contains(AmmoType))
	{
		int32 CarriedAmmo = AmmoMap[AmmoType];
		const int32 MagEmptySpace = EquippedWeapon->GetMagazineCapacity() - EquippedWeapon->GetAmmo();

		if (MagEmptySpace > CarriedAmmo)
		{
			EquippedWeapon->ReloadAmmo(CarriedAmmo);
			CarriedAmmo = 0;
			AmmoMap.Add(AmmoType, CarriedAmmo);
		}
		else
		{
			EquippedWeapon->ReloadAmmo(MagEmptySpace);
			CarriedAmmo -= MagEmptySpace;
			AmmoMap.Add(AmmoType, CarriedAmmo);
		}
	}
}

bool AShooterCharacter::CarryingAmmo()
{
	if (EquippedWeapon == nullptr)
		return false;

	auto AmmoType = EquippedWeapon->GetAmmoType();

	if (AmmoMap.Contains(AmmoType))
		return AmmoMap[AmmoType] > 0;

	return false;
}

void AShooterCharacter::GrabClip()
{
	if (EquippedWeapon == nullptr)
		return;

	if (HandSceneComponent == nullptr)
		return;

	int32 ClipBoneIndex { EquippedWeapon->GetItemMesh()->GetBoneIndex(EquippedWeapon->GetClipBoneName()) };
	ClipTransform = EquippedWeapon->GetItemMesh()->GetBoneTransform(ClipBoneIndex);

	FAttachmentTransformRules AttachmentRules(EAttachmentRule::KeepRelative, true);
	HandSceneComponent->AttachToComponent(GetMesh(), AttachmentRules, FName(TEXT("hand_l")));
	HandSceneComponent->SetWorldTransform(ClipTransform);

	EquippedWeapon->SetMovingClip(true);
}

void AShooterCharacter::ReleaseClip()
{
	EquippedWeapon->SetMovingClip(false);
}

bool AShooterCharacter::GetBeamEndLocation(const FVector& MuzzleSocketLocation, FVector& OutBeamLocation)
{
	// check for crosshair trace hit
	FHitResult CrosshairHitResult;
	bool bCrosshairHit = TraceUnderCrosshairs(CrosshairHitResult, OutBeamLocation);

	if (bCrosshairHit)
		OutBeamLocation = CrosshairHitResult.Location;

	FHitResult WeaponTraceHit;
	const FVector WeaponTraceStart { MuzzleSocketLocation };
	const FVector StartToEnd { OutBeamLocation - MuzzleSocketLocation };
	const FVector WeaponTraceEnd { MuzzleSocketLocation + StartToEnd * 1.25f };
	GetWorld()->LineTraceSingleByChannel(WeaponTraceHit, WeaponTraceStart, WeaponTraceEnd, ECC_Visibility);

	if (WeaponTraceHit.bBlockingHit)
	{
		OutBeamLocation = WeaponTraceHit.Location;
		return true;
	}

	return false;
}

void AShooterCharacter::AimingButtonPressed()
{
	bAiming = true;
}

void AShooterCharacter::AimingButtonReleased()
{
	bAiming = false;
}

void AShooterCharacter::CameraInterpZoom(float DeltaTime)
{
	CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, bAiming ? CameraZoomedFOV : CameraDefaultFOV,
		DeltaTime, ZoomInterpSpeed);
	GetFollowCamera()->SetFieldOfView(CameraCurrentFOV);
}

void AShooterCharacter::SetLookRates()
{
	if (bAiming)
	{
		BaseTurnRate = AimingTurnRate;
		BaseLookUpRate = AimingLookUpRate;
	}
	else
	{
		BaseTurnRate = HipTurnRate;
		BaseLookUpRate = HipLookUpRate;
	}
}

void AShooterCharacter::CalculateCrosshairSpread(float DeltaTime)
{
	FVector2D WalkSpeedRange { 0.f, 600.f };
	FVector2D VelocityMultiplierRange { 0.f, 1.f };
	FVector Velocity { GetVelocity() };

	// calculate crosshair velocity factor
	CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(
		WalkSpeedRange,
		VelocityMultiplierRange,
		Velocity.Size());

	// calculate crosshair in air factor
	if (GetCharacterMovement()->IsFalling())
	{
		CrosshairInAirFactor = FMath::FInterpTo(
			CrosshairInAirFactor,
			2.25f,
			DeltaTime,
			2.25f);
	}
	else
	{
		CrosshairInAirFactor = FMath::FInterpTo(
			CrosshairInAirFactor,
			0.f,
			DeltaTime,
			30.f);
	}

	// calculate crosshair while aiming factor
	if (bAiming)
		CrosshairAimFactor = FMath::FInterpTo(
			CrosshairAimFactor,
			-0.4f,
			DeltaTime,
			25.f);
	else
		CrosshairAimFactor = FMath::FInterpTo(
			CrosshairAimFactor,
			0.f,
			DeltaTime,
			25.f);

	if (bFiringBullet)
		CrosshairShootingFactor = FMath::FInterpTo(
			CrosshairShootingFactor,
			.3f,
			DeltaTime,
			60.f);
	else
		CrosshairShootingFactor = FMath::FInterpTo(
			CrosshairShootingFactor,
			0.f,
			DeltaTime,
			60.f);
	
	CrosshairSpreadMultiplier = 0.5f +
		CrosshairVelocityFactor +
		CrosshairInAirFactor +
		CrosshairAimFactor +
		CrosshairShootingFactor;
}

void AShooterCharacter::StartCrosshairBulletFire()
{
	bFiringBullet = true;

	GetWorldTimerManager().SetTimer(
		CrosshairShootTimer,
		this,
		&AShooterCharacter::FinishCrosshairBulletFire,
		ShootTimeDuration);
}

void AShooterCharacter::FinishCrosshairBulletFire()
{
	bFiringBullet = false;
}

void AShooterCharacter::FireButtonPressed()
{
	bFireButtonPressed = true;
	
	FireWeapon();
}

void AShooterCharacter::FireButtonReleased()
{
	bFireButtonPressed = false;
}

void AShooterCharacter::StartFireTimer()
{
	CombatState = ECombatState::ECS_FireTimerInProgress;

	GetWorldTimerManager().SetTimer(
			AutoFireTimer,
			this,
			&AShooterCharacter::AutoFireReset,
			AutomaticFireRate);
}

void AShooterCharacter::AutoFireReset()
{
	CombatState = ECombatState::ECS_Unoccupied;

	if (WeaponHasAmmo())
	{
		if (bFireButtonPressed)
			FireWeapon();
	}
	else
	{
		ReloadWeapon();
	}
}

bool AShooterCharacter::TraceUnderCrosshairs(FHitResult& OutHitResult, FVector& OutHitLocation)
{
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
		GEngine->GameViewport->GetViewportSize(ViewportSize);

	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;

	bool bScreenToWorld =
		UGameplayStatics::DeprojectScreenToWorld(UGameplayStatics::GetPlayerController(this, 0),
		CrosshairLocation,
		CrosshairWorldPosition,
		CrosshairWorldDirection);

	if (bScreenToWorld)
	{
		const FVector Start { CrosshairWorldPosition };
		const FVector End { Start + CrosshairWorldDirection * 50000.f };
		OutHitLocation = End;
		GetWorld()->LineTraceSingleByChannel(OutHitResult, Start, End, ECC_Visibility);

		if (OutHitResult.bBlockingHit)
		{
			OutHitLocation = OutHitResult.Location;
			return true;
		}
	}

	return false;
}

void AShooterCharacter::TraceForItems()
{
	if (bShouldTraceForItems)
	{
		FHitResult ItemTraceResult;
		FVector HitLocation;
		TraceUnderCrosshairs(ItemTraceResult, HitLocation);
	
		if (ItemTraceResult.bBlockingHit)
		{
			TraceHitItem = Cast<AItem>(ItemTraceResult.Actor);
			if (TraceHitItem && TraceHitItem->GetPickupWidget())
				TraceHitItem->GetPickupWidget()->SetVisibility(true);

			if (TraceHitItemLastFrame)
				if (TraceHitItem != TraceHitItemLastFrame)
					TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);

			TraceHitItemLastFrame = TraceHitItem;
		}
	}
	else if (TraceHitItemLastFrame)
		TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
}

AWeapon* AShooterCharacter::SpawnDefaultWeapon()
{
	if (DefaultWeaponClass)
		return GetWorld()->SpawnActor<AWeapon>(DefaultWeaponClass);

	return nullptr;
}

void AShooterCharacter::EquipWeapon(AWeapon* WeaponToEquip)
{
	if (WeaponToEquip)
	{
		const USkeletalMeshSocket* HandSocket = GetMesh()->GetSocketByName(FName("RightHandSocket"));

		if (HandSocket)
			HandSocket->AttachActor(WeaponToEquip, GetMesh());

		EquippedWeapon = WeaponToEquip;
		EquippedWeapon->SetItemState(EItemState::EIS_Equipped);
	}
}

void AShooterCharacter::DropWeapon()
{
	if (EquippedWeapon)
	{
		FDetachmentTransformRules DetachmentTransformRules(EDetachmentRule::KeepWorld, true); 
		EquippedWeapon->GetItemMesh()->DetachFromComponent(DetachmentTransformRules);

		EquippedWeapon->SetItemState(EItemState::EIS_Falling);
		EquippedWeapon->ThrowWeapon();
	}
}

void AShooterCharacter::SelectButtonPressed()
{
	if (TraceHitItem)
	{
		TraceHitItem->StartItemCurve(this);

		if (TraceHitItem->GetPickupSound())
		{
			UGameplayStatics::PlaySound2D(this, TraceHitItem->GetPickupSound());
		}
	}
}

void AShooterCharacter::SelectButtonReleased()
{
}

void AShooterCharacter::SwapWeapon(AWeapon* WeaponToSwap)
{
	DropWeapon();
	EquipWeapon(WeaponToSwap);
	TraceHitItem = nullptr;
	TraceHitItemLastFrame = nullptr;
}

void AShooterCharacter::InitializeAmmoMap()
{
	AmmoMap.Add(EAmmoType::EAT_9mm, Starting9mmAmmo);
	AmmoMap.Add(EAmmoType::EAT_AR, StartingARAmmo);
}

bool AShooterCharacter::WeaponHasAmmo()
{
	if (EquippedWeapon == nullptr) return false;

	return EquippedWeapon->GetAmmo() > 0;
}

void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	CameraInterpZoom(DeltaTime);
	SetLookRates();
	CalculateCrosshairSpread(DeltaTime);
	TraceForItems();
	
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("TurnRate", this, &AShooterCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AShooterCharacter::LookUpAtRate);
	PlayerInputComponent->BindAxis("Turn", this, &AShooterCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AShooterCharacter::LookUp);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("FireButton", IE_Pressed, this, &AShooterCharacter::FireButtonPressed);
	PlayerInputComponent->BindAction("FireButton", IE_Released, this, &AShooterCharacter::FireButtonReleased);

	PlayerInputComponent->BindAction("AimButton", IE_Pressed, this, &AShooterCharacter::AimingButtonPressed);
	PlayerInputComponent->BindAction("AimButton", IE_Released, this, &AShooterCharacter::AimingButtonReleased);

	PlayerInputComponent->BindAction("Select", IE_Pressed, this, &AShooterCharacter::SelectButtonPressed);
	PlayerInputComponent->BindAction("Select", IE_Released, this, &AShooterCharacter::SelectButtonReleased);

	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &AShooterCharacter::ReloadButtonPressed);
}

float AShooterCharacter::GetCrosshairSpreadMultiplier() const
{
	return CrosshairSpreadMultiplier;
}

void AShooterCharacter::IncrementOverlappedItemCount(int8 Amount)
{
	if (OverlappedItemCount + Amount <= 0)
	{
		OverlappedItemCount = 0;
		bShouldTraceForItems = false;
	}
	else
	{
		OverlappedItemCount += Amount;
		bShouldTraceForItems = true;
	}
}

FVector AShooterCharacter::GetCameraInterpLocation()
{
	const FVector CameraWorldLocation { FollowCamera->GetComponentLocation() };
	const FVector CameraForward { FollowCamera->GetForwardVector() };

	return CameraWorldLocation + CameraForward * CameraInterpDistance +
		FVector(0.f, 0.f, CameraInterpElevation);
}

void AShooterCharacter::GetPickupItem(AItem* Item)
{
	if (Item->GetEquipSound())
		UGameplayStatics::PlaySound2D(this, Item->GetEquipSound());
	
	auto Weapon = Cast<AWeapon>(Item);
	if (Weapon)
	{
		SwapWeapon(Weapon);
	}
}

