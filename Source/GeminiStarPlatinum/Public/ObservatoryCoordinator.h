// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ObservatoryCoordinator.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnControlModeChanged);

UENUM(BlueprintType) enum class EControlMode : uint8 { Manual, Live };

/**
 * Contains general movement rules coordinated between dome, telescope; Manages data access to TCS Epics API
 */

UCLASS()
class GEMINISTARPLATINUM_API UObservatoryCoordinator : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly) EControlMode Mode = EControlMode::Manual;
	UPROPERTY(BlueprintAssignable) FOnControlModeChanged OnControlModeChanged;

	UFUNCTION(BlueprintCallable) void SetControlMode(EControlMode NewMode);
};
