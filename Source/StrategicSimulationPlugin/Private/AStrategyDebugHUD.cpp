#include "AStrategyDebugHUD.h"
#include "Engine/Engine.h"
#include "UStrategyCampaignSubsystem.h"
#include "UTimeManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "StrategicSiteDefinition.h"
#include "UStrategicSimulationDisplayHelpers.h"
#include "UFactionIntelSubsystem.h"
#include "URadarTerrainSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "UStrategyBase.h"
#include "UStrategySoldier.h"
#include "UMissionGroup.h"
#include "UMissionManagerSubsystem.h"
#include "UStrategyVehicle.h"
#include "Engine/Canvas.h"

namespace
{
    bool IsDebugExecAllowed(const UGameInstance* GameInstance)
    {
        if (!GameInstance)
        {
            return false;
        }

        const UStrategyCampaignSubsystem* Campaign = GameInstance->GetSubsystem<UStrategyCampaignSubsystem>();
        if (!Campaign)
        {
#if UE_BUILD_SHIPPING
            return false;
#else
            return true;
#endif
        }

        return Campaign->bAllowDebugExecCommands;
    }
}

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

    if (!BaseMgr || !ResourceMgr) return;

    auto ComputeFactionForceTotals = [](const TArray<UStrategyBase*>& Bases,
        int32& OutSoldiersStationed, int32& OutSoldiersOnMission,
        int32& OutVehiclesStationed, int32& OutVehiclesOnMission)
    {
        OutSoldiersStationed = 0;
        OutSoldiersOnMission = 0;
        OutVehiclesStationed = 0;
        OutVehiclesOnMission = 0;

        for (UStrategyBase* Base : Bases)
        {
            if (!Base) continue;
            OutSoldiersStationed += Base->GetStationedSoldiersCount();
            OutSoldiersOnMission += Base->GetSoldiersOnMissionCount();
            OutVehiclesStationed += Base->GetStationedVehiclesCount();
            OutVehiclesOnMission += Base->GetVehiclesOnMissionCount();
        }
    };

    auto FindCommandCenterBase = [](const TArray<UStrategyBase*>& Bases) -> UStrategyBase*
    {
        for (UStrategyBase* Base : Bases)
        {
            if (Base && Base->BaseName.ToString() == TEXT("Command Center"))
            {
                return Base;
            }
        }
        return nullptr;
    };

    FString DebugText;
    DebugText += TEXT("=== STRATEGIC SIMULATION DEBUG ===\n");
    DebugText += FString::Printf(TEXT("DATE %s\n\n"), *Campaign->GetTimeManager()->GetCurrentGameDate().ToString());

    FResourceStockpile HumanRes = ResourceMgr->GetResources(EFactionType::Human);
    FResourceStockpile EnemyRes = ResourceMgr->GetResources(EFactionType::Enemy);

    const TArray<UStrategyBase*>& HumanBases = BaseMgr->GetBases(EFactionType::Human);
    const TArray<UStrategyBase*>& EnemyBases = BaseMgr->GetBases(EFactionType::Enemy);

    int32 HumanSoldiersSt = 0, HumanSoldiersMission = 0, HumanVehiclesSt = 0, HumanVehiclesMission = 0;
    int32 EnemySoldiersSt = 0, EnemySoldiersMission = 0, EnemyVehiclesSt = 0, EnemyVehiclesMission = 0;
    ComputeFactionForceTotals(HumanBases, HumanSoldiersSt, HumanSoldiersMission, HumanVehiclesSt, HumanVehiclesMission);
    ComputeFactionForceTotals(EnemyBases, EnemySoldiersSt, EnemySoldiersMission, EnemyVehiclesSt, EnemyVehiclesMission);

    DebugText += FString::Printf(
        TEXT("[AI] Human | Bases: %d | M:%d Mt:%d Bio:%d Chem:%d Exo:%d RP:%d\n")
        TEXT("  Soldiers St: %d | Soldiers Mission: %d | Vehicles St: %d | Vehicles Mission: %d\n"),
        HumanBases.Num(),
        HumanRes.Money, HumanRes.Metals, HumanRes.Biologicals,
        HumanRes.Chemicals, HumanRes.ExoticMaterial, HumanRes.ResearchPoints,
        HumanSoldiersSt, HumanSoldiersMission, HumanVehiclesSt, HumanVehiclesMission);

    DebugText += FString::Printf(
        TEXT("[AI] Enemy | Bases: %d | M:%d Mt:%d Bio:%d Chem:%d Exo:%d RP:%d\n")
        TEXT("  Soldiers St: %d | Soldiers Mission: %d | Vehicles St: %d | Vehicles Mission: %d\n\n"),
        EnemyBases.Num(),
        EnemyRes.Money, EnemyRes.Metals, EnemyRes.Biologicals,
        EnemyRes.Chemicals, EnemyRes.ExoticMaterial, EnemyRes.ResearchPoints,
        EnemySoldiersSt, EnemySoldiersMission, EnemyVehiclesSt, EnemyVehiclesMission);

    if (UStrategyBase* HumanCC = FindCommandCenterBase(HumanBases))
    {
        DebugText += TEXT("=== Human Command Center ===\n");
        AppendCommandCenterStats(HumanCC, DebugText);
        DebugText += TEXT("\n");
    }

    if (UStrategyBase* EnemyCC = FindCommandCenterBase(EnemyBases))
    {
        DebugText += TEXT("=== Enemy Command Center ===\n");
        AppendCommandCenterStats(EnemyCC, DebugText);
    }

    GEngine->AddOnScreenDebugMessage(999, 0.0f, FColor::Cyan, DebugText);
}

FString AStrategyDebugHUD::BuildFacilityListText(UStrategyBase* Base)
{
    if (!Base) return FString();

    FString FacilityList;
    const TArray<EFacilityType> TypesToShow = {
        EFacilityType::Command, EFacilityType::LivingQuarters, EFacilityType::Laboratory,
        EFacilityType::Workshop, EFacilityType::Hanger, EFacilityType::Medical,
        EFacilityType::VehicleRepair, EFacilityType::Containment, EFacilityType::Autopsy
    };

    for (EFacilityType Type : TypesToShow)
    {
        const int32 Count = Base->GetTotalBuiltOfType(Type);
        if (Count > 0)
        {
            if (!FacilityList.IsEmpty())
            {
                FacilityList += TEXT(", ");
            }
            FacilityList += UStrategicSimulationDisplayHelpers::FormatFacilityCount(Type, Count);
        }
    }

    return FacilityList;
}

void AStrategyDebugHUD::AppendCommandCenterStats(UStrategyBase* Base, FString& DebugText)
{
    if (!Base) return;

    DebugText += FString::Printf(TEXT("Base: %s | Net Power: %d\n"),
        *Base->BaseName.ToString(), Base->GetNetPower());

    const FString FacilityList = BuildFacilityListText(Base);
    DebugText += FString::Printf(TEXT("  Facilities: %s\n"), *FacilityList);
    DebugText += FString::Printf(TEXT("  Soldiers stationed: %d | Vehicles stationed: %d | POW Count: %d | KIA Bodies: %d\n"),
        Base->GetStationedSoldiersCount(), Base->GetStationedVehiclesCount(),
        Base->GetPOWCount(), Base->GetKIABodyCount());
    DebugText += FString::Printf(TEXT("  Soldiers on mission: %d | Vehicles on mission: %d\n"),
        Base->GetSoldiersOnMissionCount(), Base->GetVehiclesOnMissionCount());
}

void AStrategyDebugHUD::ToggleDebugHUD()
{
    if (!IsDebugExecAllowed(GetGameInstance()))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[DEBUG HUD] ToggleDebugHUD blocked — set bAllowDebugExecCommands on campaign or initializer"));
        return;
    }

    bDebugVisible = !bDebugVisible;
    UE_LOG(LogTemp, Display, TEXT("Debug HUD %s"), bDebugVisible ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void AStrategyDebugHUD::ToggleStrategyMap()
{
    if (!IsDebugExecAllowed(GetGameInstance()))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[DEBUG HUD] ToggleStrategyMap blocked — set bAllowDebugExecCommands on campaign or initializer"));
        return;
    }

    bShowStrategyMap = !bShowStrategyMap;
    UE_LOG(LogTemp, Display, TEXT("[DEBUG HUD] Strategy Map %s"), bShowStrategyMap ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void AStrategyDebugHUD::ShowSiteInfo(int32 SiteIndex)
{
    if (!IsDebugExecAllowed(GetGameInstance()))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[DEBUG HUD] ShowSiteInfo blocked — set bAllowDebugExecCommands on campaign or initializer"));
        return;
    }

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
    if (!IsDebugExecAllowed(GetGameInstance()))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[DEBUG HUD] ClearSiteInfo blocked — set bAllowDebugExecCommands on campaign or initializer"));
        return;
    }

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

    const TArray<UMissionGroup*> ActiveMissionSnapshot = MissionMgr ? MissionMgr->ActiveMissions : TArray<UMissionGroup*>();

    for (UMissionGroup* Mission : ActiveMissionSnapshot)
    {
        DrawMission(Mission);
    }

    for (UMissionGroup* Mission : ActiveMissionSnapshot)
    {
        if (!Mission)
        {
            continue;
        }

        for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
        {
            if (Vehicle)
            {
                DrawVehicle(Vehicle);
            }
        }
    }

    // === PHASE 1: Draw ALL potential nodes + fog-of-war markers ===
    DrawRadarBlockerZones();
    DrawAllPotentialSites();

    DrawInspectedSiteHighlight();

    // Legend (updated)
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("BLUE  = Human Bases"), 50, 50, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("RED   = Enemy Bases"), 50, 80, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("GREEN = Human Vehicles  |  RED = Enemy Vehicles"), 50, 110, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("Yellow lines = Active Mission paths"), 50, 140, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("WHITE SQUARE = Node | Blue/Red dots = Discovered by faction"), 50, 170, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("BLUE/RED TRIANGLE = Salvage Wreck (destroyed vehicle faction)"), 50, 200, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("GRAY = Radar LOS blocker zones (mountains)"), 50, 230, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("CYAN RING = Command Center passive radar range"), 50, 260, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("Press ToggleSiteInfo to show resource info on sites"), 50, 290, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("YELLOW SQUARE = Inspected Site"), 50, 320, 1.0f, 1.0f, FFontRenderInfo());

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
                InspectorText += FString::Printf(TEXT("Type: %s\n"),
                    *UStrategicSimulationDisplayHelpers::GetSiteTypeDisplayName(Site->SiteType).ToString());
                InspectorText += FString::Printf(TEXT("Status: %s\n"),
                    *UStrategicSimulationDisplayHelpers::GetSiteStatusDisplayText(Site));

                bool bHumanDiscovered = BaseManager->DiscoveredSitesHuman.Contains(Site);
                bool bEnemyDiscovered = BaseManager->DiscoveredSitesEnemy.Contains(Site);
                InspectorText += FString::Printf(TEXT("Discovered: Human: %s | Enemy: %s\n"),
                    bHumanDiscovered ? TEXT("Yes") : TEXT("No"),
                    bEnemyDiscovered ? TEXT("Yes") : TEXT("No"));

                if (Site->SiteType == EStrategySiteType::SalvageSite)
                {
                    InspectorText += FString::Printf(TEXT("Wreck Owner: %s\n"),
                        *StaticEnum<EFactionType>()->GetDisplayNameTextByValue(static_cast<int64>(Site->WreckOwnerFaction)).ToString());

                    FString KnownList;
                    for (const EFactionType KnownFaction : Site->KnownFactions)
                    {
                        if (!KnownList.IsEmpty())
                        {
                            KnownList += TEXT(", ");
                        }
                        KnownList += StaticEnum<EFactionType>()->GetNameStringByValue(static_cast<int64>(KnownFaction));
                    }
                    InspectorText += FString::Printf(TEXT("Known Factions: %s\n"),
                        KnownList.IsEmpty() ? TEXT("None") : *KnownList);

                    const int32 DaysRemaining = BaseManager->GetSalvageDaysRemaining(Site);
                    InspectorText += FString::Printf(TEXT("Days Remaining: %d (expires day %d)\n"),
                        DaysRemaining, Site->SalvageExpiresOnDay);
                    InspectorText += FString::Printf(TEXT("Crew — KIA (crash): %d | MIA: %d\n"),
                        Site->KIACrashCount, Site->MIASoldiers.Num());

                    if (Site->MIASoldiers.Num() > 0)
                    {
                        InspectorText += TEXT("MIA: ");
                        for (int32 M = 0; M < Site->MIASoldiers.Num(); ++M)
                        {
                            if (UStrategySoldier* MIA = Site->MIASoldiers[M])
                            {
                                if (M > 0) InspectorText += TEXT(", ");
                                InspectorText += MIA->SoldierName;
                            }
                        }
                        InspectorText += TEXT("\n");
                    }
                }

                InspectorText += FString::Printf(TEXT("Salvage Resources (ground) — M: %d Mt: %d Bio: %d Chem: %d Exo: %d\n"),
                    Site->CurrentResources.Money,
                    Site->CurrentResources.Metals,
                    Site->CurrentResources.Biologicals,
                    Site->CurrentResources.Chemicals,
                    Site->CurrentResources.ExoticMaterial);

                if (UFactionIntelSubsystem* IntelMgr = GetGameInstance()->GetSubsystem<UFactionIntelSubsystem>())
                {
                    if (IntelMgr->IsStaleIntelEnabled())
                    {
                        auto AppendIntelLine = [&](EFactionType Faction, const TCHAR* Label)
                        {
                            const FResourceStockpile IntelRes = IntelMgr->GetDisplayResources(Faction, Site);
                            const bool bFresh = IntelMgr->IsIntelFresh(Faction, Site);
                            InspectorText += FString::Printf(
                                TEXT("%s intel — M: %d Mt: %d Chem: %d Exo: %d (%s)\n"),
                                Label,
                                IntelRes.Money, IntelRes.Metals, IntelRes.Chemicals, IntelRes.ExoticMaterial,
                                bFresh ? TEXT("fresh") : TEXT("stale"));
                        };

                        AppendIntelLine(EFactionType::Human, TEXT("Human"));
                        AppendIntelLine(EFactionType::Enemy, TEXT("Enemy"));
                    }
                }

                InspectorText += TEXT("\n");

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

                    InspectorText += FString::Printf(TEXT("Facilities: %s\n"), *BuildFacilityListText(OwningBase));

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
                    50, Canvas->SizeY - 450, 1.0f, 1.0f, FFontRenderInfo());
            }
        }
    }
}

void AStrategyDebugHUD::DrawInspectedSiteHighlight()
{
    if (!Canvas || SelectedSiteIndex < 0) return;

    UBaseManagerSubsystem* BaseManager = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseManager || !BaseManager->AllPotentialSites.IsValidIndex(SelectedSiteIndex)) return;

    UStrategySiteDefinition* Site = BaseManager->AllPotentialSites[SelectedSiteIndex];
    if (!Site) return;

    const float Scale = GetCurrentMapScale();
    const FVector2D ScreenPos = GetScreenPosition(Site->Location);

    const float BaseSize = 20.0f * Scale;
    const float HighlightSize = BaseSize * 1.6f;
    Canvas->K2_DrawBox(
        ScreenPos - FVector2D(HighlightSize * 0.5f, HighlightSize * 0.5f),
        FVector2D(HighlightSize, HighlightSize),
        3.0f * Scale,
        FLinearColor::Yellow);
}

void AStrategyDebugHUD::DrawSiteTriangle(const FVector2D& ScreenPos, float Size, float LineThickness, const FLinearColor& Color)
{
    if (!Canvas) return;

    const FVector2D Top(ScreenPos.X, ScreenPos.Y - Size * 0.5f);
    const FVector2D BottomLeft(ScreenPos.X - Size * 0.5f, ScreenPos.Y + Size * 0.5f);
    const FVector2D BottomRight(ScreenPos.X + Size * 0.5f, ScreenPos.Y + Size * 0.5f);

    Canvas->K2_DrawLine(Top, BottomLeft, LineThickness, Color);
    Canvas->K2_DrawLine(BottomLeft, BottomRight, LineThickness, Color);
    Canvas->K2_DrawLine(BottomRight, Top, LineThickness, Color);
}

void AStrategyDebugHUD::DrawSalvageSite(UStrategySiteDefinition* Site, int32 SiteIndex, float Scale, const UBaseManagerSubsystem* BaseManager)
{
    if (!Site || !Canvas || Site->SalvageState != ESalvageSiteState::Active)
    {
        return;
    }

    const FVector2D ScreenPos = GetScreenPosition(Site->Location);
    const float NodeSize = 10.0f * Scale;
    const FLinearColor WreckColor = UStrategicSimulationDisplayHelpers::GetSalvageWreckColor(Site->WreckOwnerFaction);

    DrawSiteTriangle(ScreenPos, NodeSize, 2.0f * Scale, WreckColor);

    const FString IndexText = FString::Printf(TEXT("%d"), SiteIndex);
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString(IndexText),
        ScreenPos.X + 10.0f * Scale, ScreenPos.Y - 14.0f * Scale,
        0.85f, 0.85f, FFontRenderInfo());

    if (BaseManager)
    {
        const float MarkerSize = 4.0f * Scale;
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

void AStrategyDebugHUD::DrawRadarBlockerZones()
{
    if (!Canvas || !bShowStrategyMap)
    {
        return;
    }

    URadarTerrainSubsystem* TerrainMgr = GetGameInstance()->GetSubsystem<URadarTerrainSubsystem>();
    if (!TerrainMgr || !TerrainMgr->IsRadarLOSEnabled())
    {
        return;
    }

    const float Scale = GetCurrentMapScale();
    const FLinearColor BlockerColor(0.45f, 0.45f, 0.45f, 0.55f);

    for (const FRadarBlockerZone& Zone : TerrainMgr->GetBlockerZones())
    {
        if (Zone.Shape == ERadarBlockerShape::Circle)
        {
            const FVector2D ScreenCenter = GetScreenPosition(Zone.Center);
            const float ScreenRadius = Zone.Radius * Scale;
            const int32 Segments = 24;
            FVector2D PreviousPoint = ScreenCenter + FVector2D(ScreenRadius, 0.0f);

            for (int32 Segment = 1; Segment <= Segments; ++Segment)
            {
                const float Angle = (2.0f * PI * Segment) / Segments;
                const FVector2D NextPoint = ScreenCenter + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * ScreenRadius;
                Canvas->K2_DrawLine(PreviousPoint, NextPoint, 2.0f, BlockerColor);
                PreviousPoint = NextPoint;
            }
        }
        else
        {
            const FVector2D TopLeft = GetScreenPosition(Zone.Center - Zone.HalfExtent);
            const FVector2D BottomRight = GetScreenPosition(Zone.Center + Zone.HalfExtent);
            const FVector2D Size = BottomRight - TopLeft;
            Canvas->K2_DrawBox(TopLeft, Size, 2.0f, BlockerColor);
        }
    }
}

void AStrategyDebugHUD::DrawAllPotentialSites()
{
    if (!Canvas || !bShowStrategyMap) return;

    UBaseManagerSubsystem* BaseManager = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseManager) return;

    const float Scale = GetCurrentMapScale();

    for (int32 i = 0; i < BaseManager->AllPotentialSites.Num(); i++)
    {
        UStrategySiteDefinition* Site = BaseManager->AllPotentialSites[i];
        if (!Site) continue;

        if (Site->SiteType == EStrategySiteType::SalvageSite)
        {
            DrawSalvageSite(Site, i, Scale, BaseManager);
            continue;
        }

        const FVector2D ScreenPos = GetScreenPosition(Site->Location);

        const float NodeSize = 10.0f * Scale;
        Canvas->K2_DrawBox(ScreenPos - FVector2D(NodeSize * 0.5f, NodeSize * 0.5f),
            FVector2D(NodeSize, NodeSize), 1.0f, FLinearColor(0.85f, 0.85f, 0.85f, 0.8f));

        const FString IndexText = FString::Printf(TEXT("%d"), i);
        Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString(IndexText),
            ScreenPos.X + 10.0f * Scale, ScreenPos.Y - 14.0f * Scale,
            0.85f, 0.85f, FFontRenderInfo());

        const float MarkerSize = 4.0f * Scale;
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

    const bool bIsHuman = Vehicle->HomeBase && Vehicle->HomeBase->OwningFaction == EFactionType::Human;
    const FLinearColor VehicleColor = bIsHuman
        ? FLinearColor(0.55f, 0.78f, 1.0f, 1.0f)
        : FLinearColor(1.0f, 0.55f, 0.72f, 1.0f);

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
        const FLinearColor RadarColor = bIsHuman
            ? FLinearColor(0.72f, 0.86f, 1.0f, 0.42f)
            : FLinearColor(1.0f, 0.72f, 0.86f, 0.42f);

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

    if (Base->HasOperationalCommandCenter())
    {
        UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
        if (Campaign && Campaign->bBasePassiveRadarEnabled)
        {
            const float RadarRange = Campaign->BaseRadarRangePixels * Scale;
            const bool bHumanBase = Base->OwningFaction == EFactionType::Human;
            const FLinearColor RadarColor = bHumanBase
                ? FLinearColor(0.84f, 0.93f, 1.0f, 0.45f)
                : FLinearColor(1.0f, 0.84f, 0.92f, 0.45f);
            const int32 Segments = 32;
            FVector2D PreviousPoint = ScreenPos + FVector2D(RadarRange, 0.0f);

            for (int32 Segment = 1; Segment <= Segments; ++Segment)
            {
                const float Angle = (2.0f * PI * Segment) / Segments;
                const FVector2D NextPoint = ScreenPos + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RadarRange;
                Canvas->K2_DrawLine(PreviousPoint, NextPoint, 1.0f, RadarColor);
                PreviousPoint = NextPoint;
            }
        }
    }
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
    if (Mission->bMovementActivated && Mission->VehiclesInFleet.Num() > 0)
    {
        UStrategyVehicle* LeadVehicle = Mission->VehiclesInFleet[0];
        if (LeadVehicle && LeadVehicle->CurrentMission != nullptr
            && !LeadVehicle->CurrentPosition.IsNearlyZero(10.f))
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

    float LogicalWidth = 1920.0f;
    float LogicalHeight = 1080.0f;

    if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UStrategyCampaignSubsystem* Campaign = GI->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            LogicalWidth = Campaign->LogicalMapWidth;
            LogicalHeight = Campaign->LogicalMapHeight;
        }
    }

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

    float LogicalWidth = 1920.0f;
    float LogicalHeight = 1080.0f;

    if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UStrategyCampaignSubsystem* Campaign = GI->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            LogicalWidth = Campaign->LogicalMapWidth;
            LogicalHeight = Campaign->LogicalMapHeight;
        }
    }

    float ScaleX = Canvas->SizeX / LogicalWidth;
    float ScaleY = Canvas->SizeY / LogicalHeight;
    return FMath::Min(ScaleX, ScaleY);   // uniform scale, preserves aspect ratio
}