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
	TIPCharacterYaw(0.f),
	TIPCharacterYawLastFrame(0.f),
	Pitch(0.f),
	bReloading(false),
	OffsetState(EOffsetState::EOS_Hip),
	CharacterRotation(FRotator(0.f)),
	CharacterRotationLastFrame(FRotator(0.f)),
	YawDelta(0.f),
	RecoilWeight(1.f),
	bTurningInPlace(false)
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
	
	if (Speed > 0 || bIsInAir)
	{
		RootYawOffset = 0.f;
		TIPCharacterYaw = ShooterCharacter->GetActorRotation().Yaw;
		TIPCharacterYawLastFrame = TIPCharacterYaw;
		RotationCurveLastFrame = 0.f;
		RotationCurve = 0.f;
		return;
	}

	TIPCharacterYawLastFrame = TIPCharacterYaw;
	TIPCharacterYaw = ShooterCharacter->GetActorRotation().Yaw;
	const float TIPYawDelta { TIPCharacterYaw - TIPCharacterYawLastFrame };

	RootYawOffset = UKismetMathLibrary::NormalizeAxis(RootYawOffset - TIPYawDelta);

	const float Turning { GetCurveValue(TEXT("Turning")) };
	if (Turning > 0)
	{
		bTurningInPlace = true;
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
	else bTurningInPlace = false;

	if (bTurningInPlace)
	{
		if (bReloading)
		{
			RecoilWeight = 1.f;
		}
		else
		{
			RecoilWeight = 0.f;
		}
	}
	else
	{
		if (bCrouching)
		{
			if (bReloading)
			{
				RecoilWeight = 1.f;
			}
			else
			{
				RecoilWeight = 0.1f;
			}
		}
		else
		{
			if (bAiming || bReloading)
			{
				RecoilWeight = 1.f;
			}
			else
			{
				RecoilWeight = 0.5f;
			}
		}
	}
}

void UShooterAnimInstance::Lean(float DeltaTime)
{
	if (ShooterCharacter == nullptr)
		return;

	CharacterRotationLastFrame = CharacterRotation;
	CharacterRotation = ShooterCharacter->GetActorRotation();

	const FRotator Delta{ UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame) };

	const float Target{ Delta.Yaw / DeltaTime };
	const float Interp{ FMath::FInterpTo(YawDelta, Target, DeltaTime, 6.f) };

	YawDelta = FMath::Clamp(Interp, -90.f, 90.f);
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
		bCrouching = ShooterCharacter->GetCrouching();
		
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

		if (bReloading)
			OffsetState = EOffsetState::EOS_Reloading;
		else if (bIsInAir)
			OffsetState = EOffsetState::EOS_InAir;
		else if (ShooterCharacter->GetAiming())
			OffsetState = EOffsetState::EOS_Aiming;
		else
			OffsetState = EOffsetState::EOS_Hip;
	}
	TurnInPlace();
	Lean(DeltaTime);
}

