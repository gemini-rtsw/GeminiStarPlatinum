// Fill out your copyright notice in the Description page of Project Settings.


#include "EnvironmentModel.h"

ETimeVerificationError UEnvironmentModel::SetCurrentTime(int32 Year, int32 Month, int32 Day, int32 Hour, int32 Minute, int32 Second)
{
	// Validate the input values
	if (Second < 0 || Second > 59) return ETimeVerificationError::InvalidSeconds;
	if (Minute < 0 || Minute > 59) return ETimeVerificationError::InvalidMinutes;
	if (Hour < 0 || Hour > 23) return ETimeVerificationError::InvalidHours;
	if (Day < 1 || Day > 31) return ETimeVerificationError::InvalidDay; // Simplified check; does not account for month length or leap years
	if (Month < 1 || Month > 12) return ETimeVerificationError::InvalidMonth;
	if (Year < 1999 || Year > 2050) return ETimeVerificationError::InvalidYear; // Checks if year is after Gemini's establishment, reasonable amount into future
	if (FDateTime::Validate(Year, Month, Day, Hour, Minute, Second, 0) == false) return ETimeVerificationError::InvalidDateCombination; // Check for valid date combination
	
	// If all values are valid, set the current time
	CurrentTime = FDateTime(Year, Month, Day, Hour, Minute, Second);
	OnStateChanged.Broadcast();
	return ETimeVerificationError::None;
}