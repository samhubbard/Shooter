// Copyright 2021 - Samuel Hubbard


#include "ShooterAnimInstance.h"

#include "ShooterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

UShooterAnimInstance::UShooterAnimInstance() :
	Speed(0.f),
	bIsInAir(false),
	bIsAccelerating(false),
	MovementOffsetYaw(0.f),
	LastMovementOffsetYaw(0.f),
	bAiming(false),
	CharacterYaw(0.f),
	CharacterYawLastFrame(0.f),
	Pitch(0.f),
	bReloading(false)
{
	
}

void UShooterAnimInstance::NativeInitializeAnimation()
{
	ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
}

void UShooterAnimInstance::TurnInPlace()
{
	if (ShooterCharacter == nullptr)
		return;

	Pitch = ShooterCharacter->GetBaseAimRotation().Pitch;
	
	if (Speed > 0)
	{
		RootYawOffset = 0.f;
		CharacterYaw = ShooterCharacter->GetActorRotation().Yaw;
		CharacterYawLastFrame = CharacterYaw;
		RotationCurveLastFrame = 0.f;
		RotationCurve = 0.f;
		return;
	}

	CharacterYawLastFrame = CharacterYaw;
	CharacterYaw = ShooterCharacter->GetActorRotation().Yaw;
	const float YawDelta { CharacterYaw - CharacterYawLastFrame };

	RootYawOffset = UKismetMathLibrary::NormalizeAxis(RootYawOffset - YawDelta);

	const float Turning { GetCurveValue(TEXT("Turning")) };
	if (Turning > 0)
	{
		RotationCurveLastFrame = RotationCurve;
		RotationCurve = GetCurveValue(TEXT("Rotation"));
		const float DeltaRotation { RotationCurve - RotationCurveLastFrame };

		// RootYawOffset > 0 -> Turning left. RootYawOffset < 0 -> turning right
		RootYawOffset > 0 ? RootYawOffset -= DeltaRotation : RootYawOffset += DeltaRotation;

		const float ABSRootYawOffset { FMath::Abs(RootYawOffset) };
		if (ABSRootYawOffset > 90)
		{
			const float YawExcess { ABSRootYawOffset - 90.f };
			RootYawOffset > 0 ? RootYawOffset -= YawExcess : RootYawOffset += YawExcess;
		}
	}
}

void UShooterAnimInstance::UpdateAnimationProperties(float DeltaTime)
{
	if (ShooterCharacter == nullptr)
	{
		ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
	}

	if (ShooterCharacter)
	{
		bReloading = ShooterCharacter->GetCombatState() == ECombatState::ECS_Reloading;
		
		FVector Velocity { ShooterCharacter->GetVelocity() };
		Velocity.Z = 0;
		Speed = Velocity.Size();

		bIsInAir = ShooterCharacter->GetCharacterMovement()->IsFalling();

		if (ShooterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0)
		{
			bIsAccelerating = true;
		}
		else
		{
			bIsAccelerating = false;
		}

		FRotator AimRotation = ShooterCharacter->GetBaseAimRotation();
		FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(ShooterCharacter->GetVelocity());
		MovementOffsetYaw = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation).Yaw;

		if (bIsAccelerating)
		{
			LastMovementOffsetYaw = MovementOffsetYaw;
		}

		bAiming = ShooterCharacter->GetAiming();

		TurnInPlace();
	}
}

