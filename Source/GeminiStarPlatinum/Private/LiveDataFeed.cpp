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

void ULiveDataFeed::Initialize(UGameInstance* InGameInstance)
{
	GameInstance = InGameInstance;
}

void ULiveDataFeed::Connect()
{
	bConnected = true;
	TimeUntilReconnect = 0.f;
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
}

void ULiveDataFeed::PollSocket()
{
	uint8 Buffer[2048];
	int32 BytesRead = 0;

	// Drain everything currently available without blocking.
	while (Socket->Recv(Buffer, sizeof(Buffer), BytesRead, ESocketReceiveFlags::None) && BytesRead > 0)
	{
		// Buffer is not null-terminated; decode exactly BytesRead bytes as UTF-8.
		FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Buffer), BytesRead);
		RxBuffer.AppendChars(Converted.Get(), Converted.Length());
	}

	// If the peer closed the connection, drop the socket so Tick schedules a reconnect.
	ESocketConnectionState State = Socket->GetConnectionState();
	if (State == SCS_ConnectionError)
	{
		UE_LOG(LogLiveFeed, Log, TEXT("LiveFeed: connection lost, will retry"));
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
		TimeUntilReconnect = ReconnectInterval;
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
		double Value;
		if (Json->TryGetNumberField(TEXT("azim"), Value)) Tel->SetAzimTarget(static_cast<float>(Value));
		if (Json->TryGetNumberField(TEXT("elev"), Value)) Tel->SetElevTarget(static_cast<float>(Value));
		if (Json->TryGetNumberField(TEXT("cass"), Value)) Tel->SetCassTarget(static_cast<float>(Value));
	}

	if (UDomeModel* Dome = GetDome())
	{
		double Value;
		if (Json->TryGetNumberField(TEXT("dome_twist"), Value)) Dome->SetDomeTwistTarget(static_cast<float>(Value));
		bool bOpen;
		if (Json->TryGetBoolField(TEXT("dome_open"), bOpen)) Dome->SetOpen(bOpen);
	}
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

TStatId ULiveDataFeed::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULiveDataFeed, STATGROUP_Tickables);
}
