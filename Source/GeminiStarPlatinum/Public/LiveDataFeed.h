// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/NoExportTypes.h"
#include "LiveDataFeed.generated.h"

class FSocket;
class UGameInstance;
class UTelescopeModel;
class UDomeModel;

/**
 * Prototype live data feed. Connects (as a TCP client) to an external Python bridge
 * that streams newline-delimited JSON positional samples, e.g.
 *   {"azim":180.0,"elev":-60.0,"cass":120.0,"dome_twist":0.0,"top_shutter":45.0,"bot_shutter":-7.5,"vent":300.0}\n
 * Each sample is parsed and pushed into UTelescopeModel / UDomeModel via their setters.
 *
 * This object is the ONLY place that knows about sockets/JSON; swapping the transport
 * (UDP, HTTP, EPICS CA) later should not touch the models or actors.
 *
 * Implements FTickableGameObject so it drains the socket each frame while connected,
 * without requiring an actor to drive it.
 */
UCLASS()
class GEMINISTARPLATINUM_API ULiveDataFeed : public UObject, public FTickableGameObject
{
	GENERATED_BODY()
public:
	/** Must be called once after creation so the feed can resolve model subsystems. */
	void Initialize(UGameInstance* InGameInstance);

	/** Opens the (non-blocking) TCP client connection to the Python bridge. */
	void Connect();
	/** Closes the connection and clears any buffered partial data. */
	void Disconnect();

	// --- FTickableGameObject ---
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bConnected; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }

	// Connection target. Defaults match the prototype Python bridge (tools/feed_bridge).
	UPROPERTY(EditAnywhere, Category = "LiveFeed") FString Host = TEXT("127.0.0.1");
	UPROPERTY(EditAnywhere, Category = "LiveFeed") int32 Port = 9100;

	/** Seconds between reconnect attempts while in the connected (Live) state. */
	UPROPERTY(EditAnywhere, Category = "LiveFeed") float ReconnectInterval = 2.f;

private:
	/** Tries to (re)open the socket. Returns true on success. */
	bool OpenSocket();
	/** Drains all available bytes, splitting on '\n' and applying each complete line. */
	void PollSocket();
	/** Parses one JSON line and pushes it to the models. Tolerant of missing/extra keys. */
	void ApplyLine(const FString& Line);

	UTelescopeModel* GetTelescope();
	UDomeModel* GetDome();

	UPROPERTY() UGameInstance* GameInstance = nullptr;
	UPROPERTY() UTelescopeModel* TelescopeModel = nullptr;
	UPROPERTY() UDomeModel* DomeModel = nullptr;

	FSocket* Socket = nullptr;

	/** True while the feed should be live (set by Connect, cleared by Disconnect). */
	bool bConnected = false;

	/** Accumulates partial reads until a full '\n'-terminated line is available. */
	FString RxBuffer;

	/** Counts down to the next reconnect attempt when the socket is lost mid-session. */
	float TimeUntilReconnect = 0.f;
};
