// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LiveDataFeed.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ObservatoryCoordinator.generated.h"

UENUM(BlueprintType) enum class EControlMode : uint8 { Manual, Live };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnControlModeChanged, EControlMode, Mode);

/**
 * Contains general movement rules coordinated between dome, telescope; Manages data access to TCS Epics API
 */

UCLASS(BlueprintType)
class GEMINISTARPLATINUM_API UObservatoryCoordinator : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly) EControlMode Mode = EControlMode::Manual;
	UPROPERTY(BlueprintAssignable) FOnControlModeChanged OnControlModeChanged;

	UFUNCTION(BlueprintCallable) void SetControlMode(EControlMode NewMode);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	UPROPERTY() ULiveDataFeed* Feed = nullptr;
};
