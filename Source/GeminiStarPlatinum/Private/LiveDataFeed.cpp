// Fill out your copyright notice in the Description page of Project Settings.


#include "LiveDataFeed.h"

#include "TelescopeModel.h"
#include "DomeModel.h"

#include "Engine/GameInstance.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Common/TcpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogLiveFeed, Log, All);

void ULiveDataFeed::SetStatus(EFeedStatus NewStatus)
{
	if (Status != NewStatus)
	{
		Status = NewStatus;
		OnStatusChanged.Broadcast(Status);
	}
}

void ULiveDataFeed::SetDataQuality(bool bNewStale, float NewAge)
{
	DataAgeSeconds = NewAge;
	if (bDataStale != bNewStale)
	{
		bDataStale = bNewStale;
		OnDataQualityChanged.Broadcast(bDataStale, DataAgeSeconds);
	}
}

void ULiveDataFeed::Initialize(UGameInstance* InGameInstance)
{
	GameInstance = InGameInstance;
}

void ULiveDataFeed::Connect()
{
	bConnected = true;
	TimeUntilReconnect = 0.f;
	ReconnectAttempts = 0;
	SetStatus(EFeedStatus::Connecting);
	OpenSocket();
}

void ULiveDataFeed::Disconnect()
{
	bConnected = false;
	RxBuffer.Empty();
	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}

	// Reset data quality for when fresh connection is made, ensure no leftover stale flag
	TimeSinceLastSample = 0.f;
	bBridgeReportedStale = false;
	SetDataQuality(false, 0.f);

	SetStatus(EFeedStatus::Disconnected);
}

bool ULiveDataFeed::OpenSocket()
{
	// Clean up any previous socket first.
	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}

	FIPv4Address Addr;
	if (!FIPv4Address::Parse(Host, Addr))
	{
		UE_LOG(LogLiveFeed, Warning, TEXT("LiveFeed: could not parse host '%s'"), *Host);
		return false;
	}

	TSharedRef<FInternetAddr> InternetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	InternetAddr->SetIp(Addr.Value);
	InternetAddr->SetPort(Port);

	FSocket* NewSocket = FTcpSocketBuilder(TEXT("LiveDataFeedSocket"))
		.AsNonBlocking()
		.AsReusable()
		.Build();

	if (!NewSocket)
	{
		UE_LOG(LogLiveFeed, Warning, TEXT("LiveFeed: failed to create socket"));
		return false;
	}

	// Non-blocking connect: returns immediately; success is confirmed by later reads.
	NewSocket->Connect(*InternetAddr);
	Socket = NewSocket;
	bEstablished = false;
	TimeConnecting = 0.f;
	RxBuffer.Empty();
	UE_LOG(LogLiveFeed, Log, TEXT("LiveFeed: connecting to %s:%d"), *Host, Port);
	return true;
}

void ULiveDataFeed::Tick(float DeltaTime)
{
	if (!bConnected)
	{
		return;
	}

	if (!Socket)
	{
		// Lost connection — count down and retry.
		TimeUntilReconnect -= DeltaTime;
		if (TimeUntilReconnect <= 0.f)
		{
			TimeUntilReconnect = ReconnectInterval;
			OpenSocket();
		}
		return;
	}

	PollSocket();

	if (bEstablished)
	{
		TimeSinceLastSample += DeltaTime;
		const bool bStale = bBridgeReportedStale || TimeSinceLastSample >= LocalStaleTimeout;
		SetDataQuality(bStale, FMath::Max(BridgeReportedAge, TimeSinceLastSample));
	}

	// Bound the time spent waiting for a connection to deliver its first byte. A
	// non-blocking connect to an absent peer never reports failure through recv(), so
	// without this an attempt would sit in Connecting/Reconnecting forever and never
	// advance ReconnectAttempts toward Failed. PollSocket may have already torn the
	// socket down, so re-check it here.
	if (Socket && !bEstablished)
	{
		TimeConnecting += DeltaTime;
		if (TimeConnecting >= ConnectTimeout)
		{
			HandleDisconnect();
		}
	}
}

void ULiveDataFeed::PollSocket()
{
	uint8 Buffer[2048];

	// Drain everything currently available without blocking. UE's FSocket::Recv on a
	// stream socket reports state through its return value, NOT GetConnectionState():
	//   true,  BytesRead > 0  -> data
	//   true,  BytesRead == 0 -> SE_EWOULDBLOCK: connected, no data this frame (the
	//                            common case, since the feed is idle between samples)
	//   false                 -> graceful peer close (recv()==0), a hard socket error,
	//                            or (on Windows) the non-blocking connect still completing
	while (true)
	{
		int32 BytesRead = 0;
		const bool bRecvOk = Socket->Recv(Buffer, sizeof(Buffer), BytesRead, ESocketReceiveFlags::None);

		if (bRecvOk)
		{
			if (BytesRead > 0)
			{
				bEstablished = true;
				SetStatus(EFeedStatus::Live);
				ReconnectAttempts = 0;

				// Buffer is not null-terminated; decode exactly BytesRead bytes as UTF-8.
				FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Buffer), BytesRead);
				RxBuffer.AppendChars(Converted.Get(), Converted.Length());
				continue;
			}

			// SE_EWOULDBLOCK: nothing more to read this frame, still connected.
			break;
		}

		// Recv failed. Once the connection has been established, any failure is a real
		// disconnect (graceful close or error). Before it is established, tolerate the
		// "connect still in progress" codes so we don't tear down mid-handshake. We only
		// consult the error code in the not-yet-established case, because a prior
		// successful recv leaves a stale last-error value that would be misleading.
		if (!bEstablished)
		{
			const ESocketErrors LastErr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode();
			const bool bStillConnecting =
				LastErr == SE_NO_ERROR ||
				LastErr == SE_EWOULDBLOCK ||
				LastErr == SE_ENOTCONN ||
				LastErr == SE_EINPROGRESS ||
				LastErr == SE_EALREADY;
			if (bStillConnecting)
			{
				break;   // handshake not finished yet; try again next tick
			}
		}

		HandleDisconnect();
		return;
	}

	// Split out complete '\n'-terminated lines; keep any trailing partial in RxBuffer.
	int32 NewlineIndex;
	while (RxBuffer.FindChar('\n', NewlineIndex))
	{
		FString Line = RxBuffer.Left(NewlineIndex);
		RxBuffer.RightChopInline(NewlineIndex + 1);
		Line.TrimStartAndEndInline();
		if (!Line.IsEmpty())
		{
			ApplyLine(Line);
		}
	}
}

void ULiveDataFeed::HandleDisconnect()
{
	UE_LOG(LogLiveFeed, Log, TEXT("LiveFeed: connection lost, will retry"));

	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}

	bEstablished = false;
	TimeConnecting = 0.f;
	RxBuffer.Empty();
	TimeUntilReconnect = ReconnectInterval;
	ReconnectAttempts++;

	// Reset data quality for when fresh connection is made, ensure no leftover stale flag
	TimeSinceLastSample = 0.f;
	bBridgeReportedStale = false;
	SetDataQuality(false, 0.f);

	SetStatus(ReconnectAttempts >= MaxReconnectAttempts ? EFeedStatus::Failed : EFeedStatus::Reconnecting);
}

void ULiveDataFeed::ApplyLine(const FString& Line)
{
	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		// Malformed line — skip it rather than tearing down the feed.
		return;
	}

	if (UTelescopeModel* Tel = GetTelescope())
	{
		double Value = 0.f;
		double Azim = Tel->AzimTarget;
		double Elev = Tel->ElevTarget;
		double Cass = Tel->CassTarget;
		if (Json->TryGetNumberField(TEXT("azim"), Value) && FMath::IsFinite(Value))
			Azim = Value;
		if (Json->TryGetNumberField(TEXT("elev"), Value) && FMath::IsFinite(Value))
			Elev = Value;
		if (Json->TryGetNumberField(TEXT("cass"), Value) && FMath::IsFinite(Value))
			Cass = Value;
		Tel->SetTargets(Azim, Elev, Cass);
	}

	if (UDomeModel* Dome = GetDome())
	{
		double Value = 0.f;
		double DomeTwist = Dome->DomeTwistTarget;
		double TopShutter = Dome->TopSSwingTarget;
		double BotShutter = Dome->BotSSwingTarget;
		double Vent = Dome->VentSlideTarget;
		if (Json->TryGetNumberField(TEXT("dome_twist"), Value) && FMath::IsFinite(Value))
			DomeTwist = Value;
		if (Json->TryGetNumberField(TEXT("top_shutter"), Value) && FMath::IsFinite(Value))
			TopShutter = Value;
		if (Json->TryGetNumberField(TEXT("bot_shutter"), Value) && FMath::IsFinite(Value))
			BotShutter = Value;
		if (Json->TryGetNumberField(TEXT("vent"), Value) && FMath::IsFinite(Value))
			Vent = Value;
		Dome->SetTargets(DomeTwist, TopShutter, BotShutter, Vent);
	}

	double AgeVal = 0.0;
	if (Json->TryGetNumberField(TEXT("age"), AgeVal) && FMath::IsFinite(AgeVal))
		BridgeReportedAge = static_cast<float>(AgeVal);
	bool bStaleVal = false;
	if (Json->TryGetBoolField(TEXT("stale"), bStaleVal))
		bBridgeReportedStale = bStaleVal;
	TimeSinceLastSample = 0.f;
}

UTelescopeModel* ULiveDataFeed::GetTelescope()
{
	if (!TelescopeModel && GameInstance)
	{
		TelescopeModel = GameInstance->GetSubsystem<UTelescopeModel>();
	}
	return TelescopeModel;
}

UDomeModel* ULiveDataFeed::GetDome()
{
	if (!DomeModel && GameInstance)
	{
		DomeModel = GameInstance->GetSubsystem<UDomeModel>();
	}
	return DomeModel;
}

float ULiveDataFeed::GetTimeUntilReconnect()
{
	return TimeUntilReconnect;
}

int32 ULiveDataFeed::GetReconnectAttempts()
{
	return ReconnectAttempts;
}

TStatId ULiveDataFeed::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULiveDataFeed, STATGROUP_Tickables);
}
