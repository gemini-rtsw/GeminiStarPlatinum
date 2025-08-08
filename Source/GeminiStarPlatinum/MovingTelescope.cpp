#include "MovingTelescope.h"

// Sets default values
AMovingTelescope::AMovingTelescope()
{
    // Set this actor to call Tick() every frame
    PrimaryActorTick.bCanEverTick = true;

    // Initialize properties and will only update when constructed
    AzimTwistTarget = 180.f;
    AzimTwistStrength = 50000.f;
    AzimVelocityTarget = 0.003f;
    AzimVelocityDamping = 20000.f;
    AzimAngularThreshold = 3.f;

    ElevSwingTarget = -60.f;
    ElevSwingStrength = 5e19f;
    ElevVelocityTarget = 0.05f;
    ElevVelocityDamping = 2e19f;
    ElevAngularThreshold = 3.f;
    ElevAngularOffset = -45.f;
    ElevAngularLimit = 45.f;

    CassTwistTarget = 120.f;
    CassTwistStrength = 5e19f;
    CassVelocityTarget = 0.07f;
    CassVelocityDamping = 2e19f;
    CassAngularThreshold = 3.f;

	// Set static meshes for components
    const ConstructorHelpers::FObjectFinder<UStaticMesh> GroundMesh(TEXT("/Engine/EngineMeshes/ParticleCube.ParticleCube"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> AzimuthMesh(TEXT("/Game/TelescopeModels/azi_reduced_vsp/StaticMeshes/azi_reduced_vsp.azi_reduced_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> ElevationMesh(TEXT("/Game/TelescopeModels/elevation_vsp/StaticMeshes/elevation_vsp.elevation_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> CassegrainMesh(TEXT("/Game/TelescopeModels/cass_vsp/StaticMeshes/cass_vsp.cass_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> FluxCapacitorMesh(TEXT("/Engine/EngineMeshes/ParticleCube.ParticleCube"));

    RootSceneComponent = CreateDefaultSubobject<USceneComponent>("RootSceneComponent");
    RootComponent = RootSceneComponent;

    // Any changes to the components will also affect the orientation of the constraints, so you may need to update the child constraints if you change the components (so +90 component pitch, -90 constraint pitch)
    Ground = CreateMeshComponent(
        "Ground",                 // Name
        RootSceneComponent,       // Parent attachment that ensures new component rotates relative to parent
        GroundMesh.Object,        // Reference to mesh
        FVector::ZeroVector,      // Location relative to parent
        FRotator(90.f, 0.f, 0.f), // Rotation relative to parent
        1e29f,                    // Mass override (kg)
        FVector::ZeroVector       // Center of mass offsetting UE-generated COM
    );
    Ground->SetWorldScale3D(FVector(1.f, 1.f, 1.f)); // Sets scale of entire telescope, and children inheret scale because of SetupAttachment()
    Ground->SetHiddenInGame(true);
    SetBaseLocked(Ground, true);

    Azim = CreateMeshComponent(
        "Azimuth",
        Ground,
        AzimuthMesh.Object,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        1000.f, // TODO: I lost the mass calculations
        FVector(-367.04f, -1.51f, 0.457f) // (-3670.4, -15.1, 4.57) mm based on SolidWorks
    );
    Elev = CreateMeshComponent(
        "Elevation",
        Azim,
        ElevationMesh.Object,
        FVector::ZeroVector,
        FRotator(0.f, 0.f, 180.f),
		8000.f, // BUG: When mass is set too high, it cannot stabilize properly.  High masses worked before when I first started, but it seems like there's a certain setting I changed that I cannot find
        FVector(29.204f, -8.765f, -22.986f) // (292.04, -229.86, -87.65) mm based on SolidWorks
    );
    Elev->SetEnableGravity(true); // Set to true for physics sake
    Cass = CreateMeshComponent(
        "Cassegrain",
        Elev,
        CassegrainMesh.Object,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        500.f,
        FVector(-472.543f, 0.475f, 0.155f) // (-3725.43, 4.75, 1.55) mm based on SolidWorks
    );
    Cass->SetEnableGravity(true); // Set to true for physics sake

	// This thing rotates forever, tricking the actor to never sleep, which allows for very slow rotations or temporary stops
    FluxCapacitor = CreateMeshComponent(
        "Flux Capacitor",
        Cass,
        FluxCapacitorMesh.Object,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        0.f,
        FVector::ZeroVector
    );

    AzimConstraint = CreateConstraintComponent(
        "Azimuth Constraint",  // Name
        Ground,                // Parent attachment
        Azim,                  // Child attachment
        FVector::ZeroVector,   // Location relative to parent
        FRotator::ZeroRotator, // Rotation relative to parent (pitch, yaw, roll)
        true,                  // Twists?
        false,                 // Swings?
        false,                 // Slides?
        0.f,                   // Swing angle
        0.f                    // Swing limit
    );
    ElevConstraint = CreateConstraintComponent(
        "Elevation Constraint",
        Azim,
        Elev,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        false,
        true,
        false,
        ElevAngularOffset,
        ElevAngularLimit
    );
    ElevConstraint->SetAngularDriveAccelerationMode(false); // Set to false for physics sake
    CassConstraint = CreateConstraintComponent(
        "Cassegrain Constraint",
        Elev,
        Cass,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        true,
        false,
        false,
        0.f,
        0.f
    );
    CassConstraint->SetAngularDriveAccelerationMode(false); // Set to false for physics sake
    FluxCapacitorConstraint = CreateConstraintComponent(
        "Flux Capacitor Constraint",
        Cass,
        FluxCapacitor,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        true,
        false,
        false,
        0.f,
        0.f
    );
    FluxCapacitorConstraint->SetAngularDriveParams(0.f, 1.f, 0.f);
    FluxCapacitorConstraint->SetAngularVelocityTarget(FVector(1.f, 0.f, 0.f)); // Set to always spin so model doesn't sleep
}

// Called when the game starts or when spawned
void AMovingTelescope::BeginPlay()
{
    Super::BeginPlay();
    DisplayCenterOfMass();
}

// Called every frame
void AMovingTelescope::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    DisplayCenterOfMass();

    FVector LinForce;
    FVector AngForce;
    ElevConstraint->GetConstraintForce(LinForce, AngForce);
    float ForceMagnitude = AngForce.Size();
    float MinValue = 0.f;
    float MaxValue = 50000.f;

    // Clamp and normalize
    float Normalized = FMath::Clamp((ForceMagnitude - MinValue) / (MaxValue - MinValue), 0.f, 1.f);

    // Interpolate between green and red
    FLinearColor GreenColor = FLinearColor::Green;
    FLinearColor RedColor = FLinearColor::Red;
    FLinearColor MixedColor = FLinearColor::LerpUsingHSV(GreenColor, RedColor, Normalized); // HSV for smoother hue blend

    // Convert to FColor for debug draw
    FColor FinalColor = MixedColor.ToFColor(true);

    FVector Direction(0.f, 0.f, 1.f);
    FVector Start = Elev->GetComponentLocation() + (Direction * -500);
    FVector End = Start + (Direction * 1000);
    DrawDebugLine(GetWorld(), Start, End, FinalColor, false, 0.f, 1, 4.f);

    FVector ElevDirection = Elev->GetForwardVector();
    DrawDebugLine(GetWorld(), Elev->GetComponentLocation() + (ElevDirection * -500.f), Elev->GetComponentLocation() + (ElevDirection * 500.f), FColor::Emerald, false, 0.f, 1, 4.f);
    if (bLaserOn)
    {
        DrawDebugLine(GetWorld(), Elev->GetComponentLocation() + (ElevDirection * 1500.f), Elev->GetComponentLocation() + (ElevDirection * 100000.f), FColor::Yellow, false, 0.f, 0, 10.f);
    }

    FVector ElevLinForce;
    FVector ElevAngForce;
    ElevConstraint->GetConstraintForce(ElevLinForce, ElevAngForce);

    DrawDebugString(GetWorld(), Elev->GetCenterOfMass() + FVector(0.f, 0.f, -60.f), Elev->GetComponentRotation().ToString(), nullptr, FColor::Red, 0.f, true, 1.f);
    DrawDebugString(GetWorld(), Cass->GetCenterOfMass() + FVector(0.f, 0.f, -60.f), Cass->GetComponentRotation().ToString(), nullptr, FColor::Yellow, 0.f, true, 1.f);
    DrawDebugString(GetWorld(), CassConstraint->GetComponentLocation() + FVector(0.f, 0.f, -400.f), ElevAngForce.ToString(), nullptr, FColor::Red, 0.f, true, 1.4f);

    // Update motion of components
    TwistComponent(AzimConstraint, AzimTwistTarget, AzimTwistStrength, AzimVelocityTarget, AzimVelocityDamping, AzimAngularThreshold);
    SwingComponent(ElevConstraint, ElevSwingTarget, ElevSwingStrength, ElevVelocityTarget, ElevVelocityDamping, ElevAngularThreshold, ElevAngularOffset);
    TwistComponent(CassConstraint, CassTwistTarget, CassTwistStrength, CassVelocityTarget, CassVelocityDamping, CassAngularThreshold);
}

// TODO: Clean this up, numbers were chosen out of a hat
void AMovingTelescope::DisplayCenterOfMass()
{
    struct FGroupCOM { FString Label; TArray<UStaticMeshComponent*> Components; FColor Color; float Size; };
    TArray<FGroupCOM> Groups = {
        { "Pivot", { Ground }, FColor::Cyan, 3.f},
        { "ElevCOM", { Elev }, FColor::Red, 1.f },
        { "CassCOM", { Cass }, FColor::Yellow, 1.f },
        { "MainCOM", { Elev, Cass }, FColor::Orange, 2.f }
    };

    for (const auto& Group : Groups)
    {
        FVector Sum = FVector::ZeroVector;
        float MassSum = 0.f;
        for (auto* Comp : Group.Components)
        {
            if (!Comp) continue;
            const float M = Comp->GetMass();
            Sum += Comp->GetCenterOfMass() * M;
            MassSum += M;
        }
        if (MassSum > KINDA_SMALL_NUMBER)
        {
            FVector COM = Sum / MassSum;
            DrawDebugSphere(GetWorld(), COM, 6.f + Group.Size, 4, Group.Color, false, 0.f, 1, Group.Size); // Draw dot at COM
            DrawDebugString(GetWorld(), COM + FVector(0.f, 0.f, -10.f), Group.Label, nullptr, Group.Color, 0.f, true, 1.4f); // COM label
        }
    }
}
