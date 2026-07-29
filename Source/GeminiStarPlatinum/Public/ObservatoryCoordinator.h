// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LiveDataFeed.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include <CelestialVaultDaySequenceActor.h>
#include "MovingTelescope.h"
#include "MovingDome.h"
#include "Kismet/GameplayStatics.h"
#include "ObservatoryCoordinator.generated.h"

/**
 * Control mode of the observatory.
 * Manual: User has control of telescope/dome; LiveDataFeed is disconnected.
 * Live:   LiveDataFeed is connected and pushing data to telescope/dome models.
 */
UENUM(BlueprintType) enum class EControlMode : uint8 { Manual, Live };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnControlModeChanged, EControlMode, Mode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFeedStatusChanged, EFeedStatus, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDataQualityChanged, bool, IsStale, float, AgeSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrackingStatusChanged, bool, IsTracking);

/**
 * Contains general movement rules coordinated between dome, telescope; Manages data access to TCS Epics API.
 * Currently contains the logic for slewing and tracking stars. Consider moving this to a different/separate script.
 * 
 * Author: Benjamin Bercasio
 */

UCLASS(BlueprintType)
class GEMINISTARPLATINUM_API UObservatoryCoordinator : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly) EControlMode Mode = EControlMode::Manual;
	UPROPERTY(BlueprintAssignable) FOnControlModeChanged OnControlModeChanged;
	UPROPERTY(BlueprintAssignable) FOnFeedStatusChanged OnFeedStatusChanged;
	UPROPERTY(BlueprintAssignable) FOnDataQualityChanged OnDataQualityChanged;
	UPROPERTY(BlueprintAssignable) FOnTrackingStatusChanged OnTrackingStatusChanged;
	UPROPERTY(BlueprintReadOnly) EFeedStatus FeedStatus = EFeedStatus::Disconnected;
	UPROPERTY(BlueprintReadOnly) bool bDataStaleness = false;
	UPROPERTY(BlueprintReadOnly) float DataAgeSeconds = 0.f;
	UPROPERTY(BlueprintReadWrite) float AzimZeroOffset = 0.f;
	/// <summary>
	/// Calibration constant added to the mapped elevation target, in degrees. Measured in-viz
	/// against known stars (cf. AMovingTelescope::GetElevSwing's -3 readout offset). Default 0.
	/// </summary>
	UPROPERTY(BlueprintReadWrite) float ElevZeroOffset = 0.f;

	/// <summary>
	/// Gets the time remaining until the LiveDataFeed's next reconnect attempt. Returns 0 if not currently reconnecting.
	/// Forwards to ULiveDataFeed::GetTimeUntilReconnect().
	/// </summary>
	/// <returns>The remaining time until next reconnect attempt in seconds.</returns>
	UFUNCTION(BlueprintCallable) float GetTimeUntilReconnect() const;
	/// <summary>
	/// Gets the amount of reconnect attempts that have been made since last successful connection. 
	/// Returns 0 if not currently reconnecting.
	/// Forwards to ULiveDataFeed::GetReconnectAttempts().
	/// </summary>
	/// <returns>Number of attempts made since last successful connection (int32).</returns>
	UFUNCTION(BlueprintCallable) int32 GetReconnectAttempts() const;
	/// <summary>
	/// Sets the control mode of the observatory.
	/// If Live, the LiveDataFeed will connect and attempt to start receiving data.
	/// If Manual, the LiveDataFeed will disconnect and control is returned to user.
	/// </summary>
	/// <param name="NewMode">New mode to set observatory to (EControlMode).</param>
	UFUNCTION(BlueprintCallable) void SetControlMode(EControlMode NewMode);

	// Star tracking public functions. Default catalogue has 10K stars. 
	// Can change to full catalogue if need demands (refer to Celestial Vault plug-in documentation)

	/// <summary>
	/// One-time slew (alt/az change) to a star stored in Celestial Vault actor's catalogue.
	/// Toggles tracking off if star name is valid.
	/// Uses manual offset from UI immediately prior to pressing Point (slew) button. 
	/// </summary>
	/// <param name="Name">Name of a star inside Celestial Vault's catalogue (FString&)</param>
	UFUNCTION(BlueprintCallable) void SlewToStar(const FString& Name);
	/// <summary>
	/// Constant slew (alt/az change) to a star stored in Celestial Vault actor's catalogue.
	/// Resolves new rotation every 0.5 seconds using game timer manager.
	/// Early exits if the input star name is the same as the one currently being tracked.
	/// </summary>
	/// <param name="Name">Name of a star inside Celsetial Vault's catalogue (FString&)</param>
	UFUNCTION(BlueprintCallable) void TrackStar(const FString& Name);
	/// <summary>
	/// Turns off tracking (clearing timer, cached star, broadcasts) or turns on tracking (sets timer)
	/// Early exits if input tracking state is the same.
	/// </summary>
	/// <param name="NewState">Tracking on(true)/off(false) (bool)</param>
	UFUNCTION(BlueprintCallable) void ToggleTracking(const bool NewState);


	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	UPROPERTY() ULiveDataFeed* Feed = nullptr;
	UPROPERTY() TWeakObjectPtr<ACelestialVaultDaySequenceActor> CelestialVault = nullptr;
	UPROPERTY() TWeakObjectPtr<AMovingTelescope> Telescope = nullptr;
	UPROPERTY() FStarInfo TrackedStar;
	UPROPERTY() bool bTracking = false;
	UPROPERTY() FTimerHandle TrackingTimer;

	/// <summary>
	/// Changes the FeedStatus property and broadcast the OnFeedStatusChanged event.
	/// Early exit if the status is unchanged.
	/// </summary>
	/// <param name="NewStatus">New feed status (EFeedStatus)</param>
	void HandleFeedStatusChanged(EFeedStatus NewStatus);
	/// <summary>
	/// Changes the bDataStaleness and DataAgeSeconds properties and broadcast the OnDataQualityChanged event.
	/// Early exit if both are unchanged.
	/// </summary>
	/// <param name="NewStale">New staleness state (bool)</param>
	/// <param name="NewAge">New age in seconds (float)</param>
	void HandleDataQualityChanged(bool NewStale, float NewAge);

	/// <summary>
	/// Matches input name to StarInfo obj in Celestial Vault catalogue.
	/// Returns corresponding StarInfo if name is valid, otherwise false
	/// </summary>
	/// <param name="Name">Name of a star inside Celestial Vault catalogue (FString&)</param>
	/// <param name="Out">Reference to StarInfo object, modify on valid name</param>
	/// <returns>Returns true if name is valid, false otherwise (bool)</returns>
	bool FindStarByName(const FString& Name, FStarInfo& Out) const;

	/// <summary>
	/// Maps an altitude above the horizon to the telescope model's ElevTarget frame
	/// (0 = zenith, -90 = horizon), i.e. Alt - 90 + ElevZeroOffset. Unclamped;
	/// UTelescopeModel::SetElevTarget clamps to [ElevTwistMin, ElevTwistMax].
	/// </summary>
	/// <param name="AltTarget">Altitude above the horizon in degrees, [0, 90] when reachable.</param>
	/// <returns>Elevation target in degrees in the model's [-90, 0] frame.</returns>
	float MapAltToElevTarget(const float AltTarget);

	/// <summary>
	/// Calculates proper slew (alt/az/dome) to star. Timer function.
	/// </summary>
	void SolveTrackingMovement();
};
