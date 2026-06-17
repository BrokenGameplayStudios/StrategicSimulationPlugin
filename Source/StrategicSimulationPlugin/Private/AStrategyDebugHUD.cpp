#include "AStrategyDebugHUD.h"
#include "Engine/Engine.h"
#include "UStrategyCampaignSubsystem.h"
#include "UTimeManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "StrategicSiteDefinition.h"
#include "USoldierManagerSubsystem.h"
#include "UStrategyBase.h"
#include "UMissionGroup.h"
#include "UMissionManagerSubsystem.h"
#include "UStrategyVehicle.h"
#include "Engine/Canvas.h"

AStrategyDebugHUD::AStrategyDebugHUD()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AStrategyDebugHUD::BeginPlay()
{
    Super::BeginPlay();
    ToggleDebugHUD();
}

void AStrategyDebugHUD::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bDebugVisible || !GEngine) return;

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!Campaign) return;

    UBaseManagerSubsystem* BaseMgr = Campaign->GetBaseManager();
    UResourceManagerSubsystem* ResourceMgr = Campaign->GetResourceManager();
    USoldierManagerSubsystem* SoldierMgr = Campaign->GetSoldierManager();
    UMissionManagerSubsystem* MissionMgr = Campaign->GetMissionManager();

    if (!BaseMgr || !ResourceMgr || !SoldierMgr) return;

    FString DebugText;
    DebugText += TEXT("=== STRATEGIC SIMULATION DEBUG ===\n");
    DebugText += FString::Printf(TEXT("DATE %s\n\n"), *Campaign->GetTimeManager()->GetCurrentGameDate().ToString());

    // === FACTION SUMMARIES ===
    FResourceStockpile HumanRes = ResourceMgr->GetResources(EFactionType::Human);
    FResourceStockpile EnemyRes = ResourceMgr->GetResources(EFactionType::Enemy);

    DebugText += FString::Printf(TEXT("[AI] Human | Bases: %d | M:%d Mt:%d Bio:%d Chem:%d Exo:%d RP:%d\n"),
        BaseMgr->GetBases(EFactionType::Human).Num(),
        HumanRes.Money, HumanRes.Metals, HumanRes.Biologicals,
        HumanRes.Chemicals, HumanRes.ExoticMaterial, HumanRes.ResearchPoints);

    DebugText += FString::Printf(TEXT("[AI] Enemy | Bases: %d | M:%d Mt:%d Bio:%d Chem:%d Exo:%d RP:%d\n\n"),
        BaseMgr->GetBases(EFactionType::Enemy).Num(),
        EnemyRes.Money, EnemyRes.Metals, EnemyRes.Biologicals,
        EnemyRes.Chemicals, EnemyRes.ExoticMaterial, EnemyRes.ResearchPoints);

    // === DETAILED BASE STATE (matching DebugPrintFullBaseState) ===
    for (EFactionType Faction : { EFactionType::Human, EFactionType::Enemy })
    {
        const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);
        if (Bases.Num() == 0) continue;

        DebugText += FString::Printf(TEXT("=== BASE STATE FOR %s (%d bases) ===\n"),
            *UEnum::GetValueAsString(Faction), Bases.Num());

        for (UStrategyBase* Base : Bases)
        {
            if (!Base) continue;

            // === Soldiers Stationed (same logic as subsystem) ===
            int32 SoldiersStationed = 0;
            if (SoldierMgr)
            {
                for (UStrategySoldier* Soldier : SoldierMgr->GetRoster(Faction))
                {
                    if (Soldier && Soldier->StationedBase == Base)
                        SoldiersStationed++;
                }
            }

            // === Soldiers on Mission ===
            int32 SoldiersOnMission = 0;
            if (MissionMgr)
            {
                for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
                {
                    if (Mission && Mission->OriginBase == Base)
                    {
                        for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
                        {
                            if (Vehicle)
                                SoldiersOnMission += Vehicle->CurrentPassengers.Num();
                        }
                    }
                }
            }

            // === Vehicles Stationed ===
            int32 VehiclesStationed = 0;
            for (UStrategyFacility* Fac : Base->Facilities)
            {
                if (Fac && Fac->FacilityDefinition &&
                    Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger &&
                    Fac->BuildProgressDays <= 0)
                {
                    VehiclesStationed += Fac->ParkedVehicles.Num();
                }
            }

            // === Vehicles on Mission ===
            int32 VehiclesOnMission = 0;
            if (MissionMgr)
            {
                for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
                {
                    if (Mission && Mission->OriginBase == Base)
                        VehiclesOnMission += Mission->VehiclesInFleet.Num();
                }
            }

            DebugText += FString::Printf(TEXT("Base: %s | Net Power: %d\n"),
                *Base->BaseName.ToString(), Base->GetNetPower());

            // Facilities
            FString FacilityList;
            TArray<EFacilityType> TypesToShow = {
                EFacilityType::Command, EFacilityType::LivingQuarters, EFacilityType::Laboratory,
                EFacilityType::Workshop, EFacilityType::Hanger, EFacilityType::Medical,
                EFacilityType::VehicleRepair, EFacilityType::Containment, EFacilityType::Autopsy
            };

            for (EFacilityType Type : TypesToShow)
            {
                int32 Count = Base->GetTotalBuiltOfType(Type);
                if (Count > 0)
                {
                    FacilityList += FString::Printf(TEXT("%d %s, "), Count, *UEnum::GetValueAsString(Type));
                }
            }
            if (!FacilityList.IsEmpty())
                FacilityList = FacilityList.LeftChop(2);

            DebugText += FString::Printf(TEXT("  Facilities: %s\n"), *FacilityList);

            DebugText += FString::Printf(TEXT("  Soldiers stationed: %d | Vehicles stationed: %d | POW Count: %d | KIA Bodies: %d\n"),
                SoldiersStationed, VehiclesStationed, Base->GetPOWCount(), Base->GetKIABodyCount());

            DebugText += FString::Printf(TEXT("  Soldiers on mission: %d | Vehicles on mission: %d\n\n"),
                SoldiersOnMission, VehiclesOnMission);
        }
    }

    GEngine->AddOnScreenDebugMessage(999, 0.0f, FColor::Cyan, DebugText);
}

void AStrategyDebugHUD::ToggleDebugHUD()
{
    bDebugVisible = !bDebugVisible;
    UE_LOG(LogTemp, Display, TEXT("Debug HUD %s"), bDebugVisible ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void AStrategyDebugHUD::ToggleStrategyMap()
{
    bShowStrategyMap = !bShowStrategyMap;
    UE_LOG(LogTemp, Display, TEXT("[DEBUG HUD] Strategy Map %s"), bShowStrategyMap ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void AStrategyDebugHUD::ShowSiteInfo(int32 SiteIndex)
{
    UBaseManagerSubsystem* BaseManager = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseManager) return;

    if (SiteIndex < 0 || SiteIndex >= BaseManager->AllPotentialSites.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Debug HUD] Invalid site index: %d"), SiteIndex);
        return;
    }

    SelectedSiteIndex = SiteIndex;
    UE_LOG(LogTemp, Display, TEXT("[Debug HUD] Now inspecting Site #%d"), SiteIndex);
}

void AStrategyDebugHUD::ClearSiteInfo()
{
    SelectedSiteIndex = -1;
    UE_LOG(LogTemp, Display, TEXT("[Debug HUD] Site inspector cleared"));
}

// ==================== PASTE THIS FULL FUNCTION OVER THE EXISTING DrawHUD() ====================
void AStrategyDebugHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!bShowStrategyMap || !Canvas) return;

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    if (!BaseMgr) return;

    // === EXISTING: Draw bases (unchanged) ===
    for (UStrategyBase* Base : BaseMgr->GetBases(EFactionType::Human))
        DrawBase(Base, FLinearColor::Blue);

    for (UStrategyBase* Base : BaseMgr->GetBases(EFactionType::Enemy))
        DrawBase(Base, FLinearColor::Red);

    // === EXISTING: Draw active missions (unchanged) ===
    if (MissionMgr)
    {
        for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
            DrawMission(Mission);
    }

    // === EXISTING: Draw live vehicles (unchanged) ===
    if (MissionMgr)
    {
        for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
        {
            if (!Mission) continue;

            for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
            {
                if (Vehicle)
                    DrawVehicle(Vehicle);
            }
        }
    }

    // === PHASE 1: Draw ALL potential nodes + fog-of-war markers ===
    DrawAllPotentialSites();

    // Legend (updated)
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("BLUE  = Human Bases"), 50, 50, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("RED   = Enemy Bases"), 50, 80, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("GREEN = Human Vehicles  |  RED = Enemy Vehicles"), 50, 110, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("Yellow lines = Active Mission paths"), 50, 140, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("WHITE SQUARE = Node | Blue/Red dots = Discovered by faction"), 50, 170, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("Press ToggleSiteInfo to show resource info on sites"), 50, 200, 1.0f, 1.0f, FFontRenderInfo());

    // === SITE INSPECTOR (Bottom of screen) ===
    if (SelectedSiteIndex >= 0)
    {
        UBaseManagerSubsystem* BaseManager = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
        if (BaseManager && BaseManager->AllPotentialSites.IsValidIndex(SelectedSiteIndex))
        {
            UStrategySiteDefinition* Site = BaseManager->AllPotentialSites[SelectedSiteIndex];
            if (Site)
            {
                FString InspectorText;

                // === SITE INFO ===
                InspectorText += FString::Printf(TEXT("=== SITE #%d INSPECTOR ===\n"), SelectedSiteIndex);
                InspectorText += FString::Printf(TEXT("Name: %s\n"), *Site->SiteName);
                InspectorText += FString::Printf(TEXT("Location: (%.0f, %.0f)\n"), Site->Location.X, Site->Location.Y);
                InspectorText += FString::Printf(TEXT("Type: %s\n"), *UEnum::GetValueAsString(Site->SiteType));

                bool bHumanDiscovered = BaseManager->DiscoveredSitesHuman.Contains(Site);
                bool bEnemyDiscovered = BaseManager->DiscoveredSitesEnemy.Contains(Site);
                InspectorText += FString::Printf(TEXT("Discovered: Human: %s | Enemy: %s\n"),
                    bHumanDiscovered ? TEXT("Yes") : TEXT("No"),
                    bEnemyDiscovered ? TEXT("Yes") : TEXT("No"));

                FString StatusText = Site->bHasBeenUsed ? TEXT("Used (Base Built)") : TEXT("Available");
                InspectorText += FString::Printf(TEXT("Status: %s\n"), *StatusText);

                // Remaining Resources on Site
                InspectorText += FString::Printf(TEXT("Remaining Resources - Mt: %d Bio: %d Chem: %d Exo: %d\n\n"),
                    Site->CurrentResources.Metals,
                    Site->CurrentResources.Biologicals,
                    Site->CurrentResources.Chemicals,
                    Site->CurrentResources.ExoticMaterial);

                // === FIND OWNING BASE ===
                UStrategyBase* OwningBase = nullptr;

                for (UStrategyBase* Base : BaseManager->GetBases(EFactionType::Human))
                {
                    if (Base && Base->BuiltOnSite == Site)
                    {
                        OwningBase = Base;
                        break;
                    }
                }

                if (!OwningBase)
                {
                    for (UStrategyBase* Base : BaseManager->GetBases(EFactionType::Enemy))
                    {
                        if (Base && Base->BuiltOnSite == Site)
                        {
                            OwningBase = Base;
                            break;
                        }
                    }
                }

                // === BASE SECTION ===
                if (OwningBase)
                {
                    FString FactionStr = (OwningBase->OwningFaction == EFactionType::Human) ? TEXT("Human") : TEXT("Enemy");
                    InspectorText += FString::Printf(TEXT("== Base (%s) ==\n"), *FactionStr);

                    // Extraction
                    FResourceStockpile Extraction = OwningBase->GetDailyExtractionFromSite();
                    InspectorText += FString::Printf(TEXT("Extraction/Day M: %d Mt: %d Bio: %d Chem: %d Exo: %d RP: %d\n"),
                        Extraction.Money, Extraction.Metals, Extraction.Biologicals,
                        Extraction.Chemicals, Extraction.ExoticMaterial, 0);

                    // Facilities
                    FString FacilityList;
                    TArray<EFacilityType> Types = {
                        EFacilityType::Command, EFacilityType::LivingQuarters, EFacilityType::Laboratory,
                        EFacilityType::Workshop, EFacilityType::Hanger, EFacilityType::Medical,
                        EFacilityType::VehicleRepair, EFacilityType::Containment, EFacilityType::Autopsy
                    };

                    for (EFacilityType Type : Types)
                    {
                        int32 Count = OwningBase->GetTotalBuiltOfType(Type);
                        if (Count > 0)
                        {
                            FacilityList += FString::Printf(TEXT("%d %s, "), Count, *UEnum::GetValueAsString(Type));
                        }
                    }

                    if (!FacilityList.IsEmpty())
                        FacilityList = FacilityList.LeftChop(2);

                    InspectorText += FString::Printf(TEXT("Facilities: %s\n"), *FacilityList);

                    // Dynamic Counts
                    int32 SoldiersStationed = OwningBase->GetStationedSoldiersCount();
                    int32 SoldiersOnMission = OwningBase->GetSoldiersOnMissionCount();
                    int32 VehiclesStationed = OwningBase->GetStationedVehiclesCount();
                    int32 VehiclesOnMission = OwningBase->GetVehiclesOnMissionCount();

                    InspectorText += FString::Printf(TEXT("Soldiers Stationed: %d | Soldiers on Mission: %d\n"),
                        SoldiersStationed, SoldiersOnMission);

                    InspectorText += FString::Printf(TEXT("Vehicles Stationed: %d | Vehicles on Mission: %d\n"),
                        VehiclesStationed, VehiclesOnMission);
                }
                else
                {
                    InspectorText += TEXT("== No Base Built on this Site ==\n");
                }

                // Draw at bottom
                Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString(InspectorText),
                    50, Canvas->SizeY - 380, 1.0f, 1.0f, FFontRenderInfo());
            }
        }
    }
}

// ==================== NEW FUNCTION - ADD THIS ANYWHERE IN THE FILE (e.g. after DrawDiscoveredSites) ====================
void AStrategyDebugHUD::DrawAllPotentialSites()
{
    if (!Canvas || !bShowStrategyMap) return;

    UBaseManagerSubsystem* BaseManager = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseManager) return;

    float Scale = GetCurrentMapScale();

    for (int32 i = 0; i < BaseManager->AllPotentialSites.Num(); i++)
    {
        UStrategySiteDefinition* Site = BaseManager->AllPotentialSites[i];
        if (!Site) continue;

        FVector2D ScreenPos = GetScreenPosition(Site->Location);

        // Site Node
        float NodeSize = 10.0f * Scale;
        Canvas->K2_DrawBox(ScreenPos - FVector2D(NodeSize * 0.5f, NodeSize * 0.5f),
            FVector2D(NodeSize, NodeSize), 1.0f, FLinearColor(0.85f, 0.85f, 0.85f, 0.8f));

        // === Site Index Number (Bigger & Clearer) ===
        FString IndexText = FString::Printf(TEXT("%d"), i);
        Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString(IndexText),
            ScreenPos.X + 10.0f * Scale, ScreenPos.Y - 14.0f * Scale,
            0.85f, 0.85f, FFontRenderInfo());   // Increased size

        // Discovery Markers
        float MarkerSize = 4.0f * Scale;
        if (BaseManager->DiscoveredSitesHuman.Contains(Site))
        {
            Canvas->K2_DrawBox(ScreenPos - FVector2D(8.0f * Scale, 12.0f * Scale),
                FVector2D(MarkerSize, MarkerSize), 1.0f, FLinearColor::Blue);
        }
        if (BaseManager->DiscoveredSitesEnemy.Contains(Site))
        {
            Canvas->K2_DrawBox(ScreenPos + FVector2D(4.0f * Scale, -12.0f * Scale),
                FVector2D(MarkerSize, MarkerSize), 1.0f, FLinearColor::Red);
        }
    }
}

void AStrategyDebugHUD::DrawVehicle(UStrategyVehicle* Vehicle)
{
    if (!Vehicle || !Canvas) return;

    float Scale = GetCurrentMapScale();

    FVector2D ScreenPos = GetScreenPosition(Vehicle->CurrentPosition);

    // Faction color
    FLinearColor VehicleColor = FLinearColor::Red;
    if (Vehicle->HomeBase)
        VehicleColor = (Vehicle->HomeBase->OwningFaction == EFactionType::Human) ? FLinearColor::Green : FLinearColor::Red;

    // Vehicle dot
    float VehicleSize = 8.0f * Scale;
    Canvas->K2_DrawBox(ScreenPos - FVector2D(VehicleSize * 0.5f, VehicleSize * 0.5f),
        FVector2D(VehicleSize, VehicleSize), 2.0f * Scale, VehicleColor);

    // Name label
    Canvas->DrawText(GEngine->GetSmallFont(),
        FText::FromString(Vehicle->VehicleDefinition ? Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("VEH")),
        ScreenPos.X + (12.0f * Scale), ScreenPos.Y - (8.0f * Scale),
        0.7f, 0.7f, FFontRenderInfo());

    FString StateText = FString::Printf(TEXT("%s / %s"),
        *UEnum::GetValueAsString(Vehicle->GetMissionPhase()),
        *UEnum::GetValueAsString(Vehicle->GetBehavior()));
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString(StateText),
        ScreenPos.X + (12.0f * Scale), ScreenPos.Y + (4.0f * Scale),
        0.55f, 0.55f, FFontRenderInfo());

    if (bShowVehiclePaths)
    {
        if (Vehicle->GetMissionPhase() == EVehicleMissionPhase::Returning && Vehicle->ReturningWaypoints.Num() >= 2)
        {
            for (int32 i = 0; i < Vehicle->ReturningWaypoints.Num() - 1; ++i)
            {
                FVector2D A = GetScreenPosition(Vehicle->ReturningWaypoints[i]);
                FVector2D B = GetScreenPosition(Vehicle->ReturningWaypoints[i + 1]);
                Canvas->K2_DrawLine(A, B, 1.5f * Scale, FLinearColor(1.0f, 0.5f, 0.0f, 1.0f));
            }
        }
        else if (Vehicle->CurrentWaypoints.Num() >= 2)
        {
            for (int32 i = 0; i < Vehicle->CurrentWaypoints.Num() - 1; ++i)
            {
                FVector2D A = GetScreenPosition(Vehicle->CurrentWaypoints[i]);
                FVector2D B = GetScreenPosition(Vehicle->CurrentWaypoints[i + 1]);
                Canvas->K2_DrawLine(A, B, 1.5f * Scale, FLinearColor::Yellow);
            }
        }
    }
        
    // === RADAR CIRCLE (always uses value from VehicleDefinition) ===
    float RadarRange = Vehicle->GetRadarRange();
    if (RadarRange > 0.0f)
    {
        float ScreenRadius = RadarRange * Scale;
        FLinearColor RadarColor(0.0f, 1.0f, 1.0f, 0.35f);

        const int32 NumSegments = 48;
        for (int32 i = 0; i < NumSegments; ++i)
        {
            float Angle1 = (float)i / NumSegments * 2.0f * PI;
            float Angle2 = (float)(i + 1) / NumSegments * 2.0f * PI;

            FVector2D P1(ScreenPos.X + FMath::Cos(Angle1) * ScreenRadius, ScreenPos.Y + FMath::Sin(Angle1) * ScreenRadius);
            FVector2D P2(ScreenPos.X + FMath::Cos(Angle2) * ScreenRadius, ScreenPos.Y + FMath::Sin(Angle2) * ScreenRadius);

            Canvas->K2_DrawLine(P1, P2, 1.0f, RadarColor);
        }

        FString RadiusText = FString::Printf(TEXT("R: %.0f"), RadarRange);
        Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString(RadiusText),
            ScreenPos.X + 15.0f * Scale, ScreenPos.Y - 20.0f * Scale,
            0.6f, 0.6f, FFontRenderInfo());
    }
}

void AStrategyDebugHUD::DrawBase(UStrategyBase* Base, FLinearColor Color)
{
    if (!Base || !Canvas) return;

    float Scale = GetCurrentMapScale();

    FVector2D ScreenPos = GetScreenPosition(Base->MapLocation);

    // Scaled base box (20x20 logical pixels → grows/shrinks with map)
    float BaseSize = 20.0f * Scale;
    Canvas->K2_DrawBox(ScreenPos - FVector2D(BaseSize * 0.5f, BaseSize * 0.5f),
        FVector2D(BaseSize, BaseSize), 2.0f * Scale, Color);

    // Scaled name label offset
    Canvas->DrawText(GEngine->GetSmallFont(),
        FText::FromString(Base->BaseName.ToString()),
        ScreenPos.X + (25.0f * Scale), ScreenPos.Y - (10.0f * Scale),
        1.0f, 1.0f, FFontRenderInfo());
}

void AStrategyDebugHUD::DrawMission(UMissionGroup* Mission)
{
    if (!Mission || !Mission->OriginBase || !Canvas) return;

    FVector2D BasePos = GetScreenPosition(Mission->OriginBase->MapLocation);

    // === Clean mission label near the originating base ===
    FString Info = FString::Printf(TEXT("%s"), *UEnum::GetValueAsString(Mission->MissionType));
    Canvas->DrawText(GEngine->GetSmallFont(),
        FText::FromString(Info),
        BasePos.X + 15.0f, BasePos.Y - 5.0f,
        0.8f, 0.8f, FFontRenderInfo());

    // === Proper faint connection line from base to the mission group ===
    // (only draws if the mission has vehicles and at least one is moving)
    if (Mission->VehiclesInFleet.Num() > 0)
    {
        UStrategyVehicle* LeadVehicle = Mission->VehiclesInFleet[0];
        if (LeadVehicle && LeadVehicle->CurrentMission != nullptr)
        {
            FVector2D VehiclePos = GetScreenPosition(LeadVehicle->CurrentPosition);

            // Very faint yellow line so it doesn't fight with the detailed vehicle path
            Canvas->K2_DrawLine(BasePos, VehiclePos, 1.0f, FLinearColor(1.0f, 1.0f, 0.0f, 0.25f));
        }
    }
}

FVector2D AStrategyDebugHUD::GetScreenPosition(const FVector2D& LogicalPos) const
{
    if (!Canvas)
    {
        // Safety fallback
        return (LogicalPos * MapScale) + MapOffset;
    }

    const float LogicalWidth = 1920.0f;
    const float LogicalHeight = 1080.0f;

    float UniformScale = GetCurrentMapScale();

    FVector2D ScaledPos = LogicalPos * UniformScale;

    FVector2D CanvasCenter(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);
    FVector2D LogicalCenter(LogicalWidth * 0.5f, LogicalHeight * 0.5f);

    return CanvasCenter + (ScaledPos - LogicalCenter * UniformScale);
}

float AStrategyDebugHUD::GetCurrentMapScale() const
{
    if (!Canvas)
    {
        return 1.0f;
    }

    const float LogicalWidth = 1920.0f;
    const float LogicalHeight = 1080.0f;

    float ScaleX = Canvas->SizeX / LogicalWidth;
    float ScaleY = Canvas->SizeY / LogicalHeight;
    return FMath::Min(ScaleX, ScaleY);   // uniform scale, preserves aspect ratio
}