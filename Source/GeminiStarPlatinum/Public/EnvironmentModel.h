// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssemblyModel.h"
#include "EnvironmentModel.generated.h"

UENUM(BlueprintType) enum class ETimeVerificationError : uint8
{
	None,                   /*< No error; time is valid and verified */
	InvalidSeconds,         /*< Seconds are out of range (0-59) */
	InvalidMinutes,         /*< Minutes are out of range (0-59) */
	InvalidHours,           /*< Hours are out of range (0-23) */
	InvalidDay,             /*< Day is out of range (1-31) */
	InvalidMonth,           /*< Month is out of range (1-12) */
	InvalidYear,            /*< Year is out of range (e.g., negative or too far in the future) */
	InvalidDateCombination, /*< The combination of year, month, and day does not form a valid date (e.g., February 30) */
};

/**
 * 
 */
UCLASS()
class GEMINISTARPLATINUM_API UEnvironmentModel : public UAssemblyModel
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly) FDateTime CurrentTime = FDateTime(2020, 1, 1, 0, 0, 1, 0);

	UFUNCTION(BlueprintCallable) ETimeVerificationError SetCurrentTime(int32 Year, int32 Month, int32 Day, int32 Hour, int32 Minute, int32 Second);
};
