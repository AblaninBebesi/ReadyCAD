#pragma once

#include <cstddef>
#include <string_view>

namespace rc::i18n {

/// Every user-visible string has an id. Code is written in English, the UI speaks Turkish.
///
/// Why an enum instead of string keys ("menu.file"): a typo becomes a compile error, and a test
/// checks that every id has a translation. Adding a language = adding one more table.
enum class TextId : std::size_t {
    // Application, menus
    AppTitle, Untitled,
    MenuFile, MenuNew, MenuOpen, MenuSave, MenuSaveAs, MenuProfile, MenuLoadProfile,
    MenuSaveProfile, MenuExit, MenuEdit, MenuUndo, MenuRedo, MenuView, MenuShowFronts,
    MenuShowHoles, MenuFitView, MenuResetLayout, MenuShowImGuiDemo, MenuHelp, MenuAbout,
    // Windows
    WindowCatalog, WindowParameters, WindowProduct, WindowViewport, WindowCutList,
    WindowWarnings, AboutWindow, AboutBody, CoreVersion, RendererInfo,
    // Catalogue
    CatalogWardrobe, CatalogWardrobeWithDrawers, CatalogChestOfDrawers, CatalogBookcase,
    CatalogHint,
    // Component kinds
    KindProduct, KindCarcass, KindSection, KindDrawerStack, KindDrawer, KindSideWall,
    KindTopPanel, KindBottomPanel, KindBackPanel, KindShelf, KindFixedShelf, KindPartition,
    KindDoor, KindDrawerFront, KindDrawerBoxSide, KindDrawerBoxEnd, KindDrawerBottom,
    // Product and construction parameters
    ProductHeader, ProfileHeader, Construction, Fasteners, Name, Width, Height, Depth,
    TopJoint, BottomJoint, JointOverlay, JointInset, BackMount, BackGroove, BackOverlay,
    BackInset, Fastener, FastenerConfirmat, FastenerEccentric, FastenerDowel, AlignmentDowels,
    DrillHandlesOnMachine,
    // Sections
    SplitColumns, SplitRows, ClearSection, FixedWidth, FixedHeight, ShelfCount, ShelfMountLabel,
    MountSupports, MountConfirmat, DoorCount, DrawerCount, Content, Splitting,
    // Parts
    FixedElevation, HingeSideLabel, Left, Right, Handle, HandleDefault, HandleNone, Orientation,
    Vertical, Horizontal, Anchor, AnchorTop, AnchorCenter, AnchorBottom, EdgeOffset,
    AnchorOffset, Delete,
    // Technical info
    Info, Size, Material, Thickness, FinishSize, CutSize, EdgeBanding, Grain, GrainKept,
    GrainFree, Holes, Grooves, NoEdge, HoleConfirmat, HoleDowel, HoleEccentric,
    HoleShelfSupport, HoleHinge, HoleGroove, HoleHandle,
    // Cut list
    ColPart, ColMaterial, ColQuantity, ColFinish, ColCut, ColEdges, ColGrain, EdgeUsage,
    ColEdgeBand, ColLength, ColPieces, TotalParts,
    // Messages and dialogs
    NoWarnings, SelectAPart, ErrorTitle, ErrorOperationFailed, UnsavedTitle, UnsavedBody,
    Save, DontSave, Cancel, Close,
    // 3D view
    ViewFit, ViewFronts, ViewHoles, ViewHint,
    // Edges of a panel
    EdgeL1, EdgeL2, EdgeW1, EdgeW2,
    // Export: menu and dialog
    MenuExport, MenuExportPdf, MenuExportDxfAll, MenuExportDxfFolder, PdfWindow, PaperSize,
    PaperOrientation, Portrait, Landscape, ReportSections, CompanyName, DesignerName,
    OpenAfterExport, ExportButton, ExportDoneTitle, ExportDoneFile, ExportDoneFiles,
    ExportFailed, OpenFile, OpenFolder,
    // Report pages and title block
    ReportTitle, PageSummary, PageCutList, PageHardware, PageAssembly, PagePart, Continued,
    TbProduct, TbSheet, TbScale, TbDate, TbCompany, TbDesigner, TbTitle,
    // Report contents
    OverallSize, CarcassSize, DistinctParts, MaterialNeed, ColArea, HoleCount,
    GrooveCount, JointCount, Warnings, ColNo, ColItem, ColSpec, HwConfirmat, HwDowel,
    HwEccentric, HwShelfSupport, HwHinge, HwHandle, HwDrawerSlide, ViewFrontClosed,
    ViewFrontOpen, ViewSection, FaceA, FaceB, ViewFrom, HoleTable, ColSide, ColDiameter,
    ColDepth, ColType, SideEdge, OriginNote, TitleBlock, HardwareList,
    // Cutting and nesting (stage 7): menu, windows
    MenuCutting, MenuRunNesting, MenuNestingSettings, MenuConsumeStock, MenuShowNesting,
    MenuShowStock, MenuExportSheetsDxf, WindowNesting, WindowStock, NestingSettingsWindow,
    // Cutting plan window
    Machine, MachinePanelSaw, MachineNestingCenter, ProductCount, Calculate, Stop,
    SettingsButton, Generation, Utilization, Computing, NestingStale, NestingEmpty, SheetsTotal,
    FromStock, FromRemnant, ToPurchase, NewRemnants, Unplaced, Remnant, SheetLabel, Stacks,
    ConsumeTitle, ConsumeBody, ConsumeDone, CacheMemory, PartsOnSheet,
    // Stock window
    ColSheetSize, ColKind, SheetFull, ColNote, AddSheet, StandardSheet, MinRemnant, StockFile,
    Length, StockEmpty,
    // Cutting settings
    SawSettings, Kerf, Trim, Stages, StackHeight, FirstCuts, AlongLength, AlongWidth,
    AutoDirection, RouterSettings, ToolDiameter, ExtraSpacing, SheetMargin, GaSettings,
    Population, Generations, MutationRate, Elites, Seed, ComputeResources, Threads,
    ThreadsHint, MemoryLimit, Defaults,
    // Cutting plan pages
    PageCutting, ColSource, CutsNote, ShowCuts, StackCount,
    // CNC programs (stage 8): comments in programs, menu, settings, window
    CamDrilling, CamMilledHoles, CamGrooves, CamContours, MenuExportCnc, MenuCncSettings,
    CncSettingsWindow, PostprocessorLabel, ZeroPlaneLabel, ZeroTable, ZeroTop, Decimals,
    LineNumbers, DrillCycles, SafeHeight, ClearanceLabel, SpoilboardCut, ClimbMilling,
    ContourTool, ToolsHeader, ToolKindLabel, ToolRouter, ToolDrill, CuttingLength, Rpm,
    FeedRate, PlungeFeed, StepDown, AddTool, ShowToolpath, EstimatedTime, ToolChangesLabel,
    HolesLabel, MilledHolesLabel, LeftoverTitle, LeftoverEdge, LeftoverBottom, LeftoverNoTool,
    CncNeedsRouter, CncProgramsDone, Minutes, SecondsShort, Preset,
    // Production pipeline (stage 9): parts left out of the cutting plan, the production package
    InCuttingPlan, ColCuttingPlan, NotInCuttingPlanNote, Yes, No, PackageTitle, SummaryOrder,
    SummaryDate, SummarySize, SummaryParts, InPlanShort, OutOfPlanShort, SummaryCutting,
    NoCuttingPlan, SawProgramsNote, SummaryCnc, TotalTime, OutOfPlanTitle, FilesTitle, FilesWord,
    PartDrawingsWord, SheetDrawingsWord, ProgramsWord, CutListCsvWord, MenuExportPackage,
    MenuExportCutListCsv, PackageDone, PackageWaiting, ChangeRefused, PartsOverlap,
    PartWithoutSize, OverlapWarning, SummaryAttention,
    // Cut list: the hand of a mirror part
    HandLeft, HandRight,
    // Kitchens: kinds, tree names, catalogue, parameters, modules, hardware
    KindKitchen, KindCabinetRow, KindRail, KindPlinth, KindWorktop, CatalogKitchen,
    BaseCabinets, WallCabinets, BaseCabinet, WallCabinet, FrontRail, RearRail, KitchenLength,
    TotalHeight, BaseDepth, KitchenSizes, BaseHeight, WallHeight, WallDepth, WorktopDepth,
    WorktopHeight, TopStyle, TopStylePanel, TopStyleFlatRails, TopStyleUprightRails, PlinthLabel,
    WorktopCutoutLabel, CutoutNone, CutoutSink, CutoutHob, CutoutWidth, CutoutDepth,
    CutoutRadius, MoveLeft, MoveRight, AddCabinet, ModuleBaseDoors, ModuleBaseDrawers,
    ModuleSink, ModuleHob, ModuleWallDoors, ModuleWallOpen, KitchenHeader, PlinthHeight,
    PlinthSetback, RailWidth, HwLeg, HwPlinthClip, HwHanger, CabinetCount, CutoutDoesNotFit,
    CutoutWarning,
    // Shapes: notches, shaped corners, openings of any part
    ShapeDoesNotFit, MachiningInCut, ShapeWarning, ShapeHeader, ShapeAdd, ShapeCornerNotch,
    ShapeEdgeNotch, ShapeCornerRound, ShapeChamfer, ShapeInnerCutout, ShapeRoundHole,
    ShapeCornerLabel, ShapeEdgeLabel, ShapeFromLabel, ShapeAlongLength, ShapeAlongWidth,
    ShapeRadius, ShapeOffset, ShapeFromEnd, ShapeNotchLength, ShapeNotchDepth, ShapeOpeningLength,
    ShapeOpeningWidth, ShapeDiameter, ShapeAutomatic, ShapeNone, AfterSawTitle, AfterSawNote,
    ShapeFromCorner, ShapeFromEdge,
    // Decors
    DecorHeader, DecorCarcass, DecorFronts, DecorWorktop, DecorLabel, DecorDefault, DecorNone,

    Count // keep last
};

enum class Language { Turkish };

void setLanguage(Language language) noexcept;
[[nodiscard]] Language currentLanguage() noexcept;

/// UTF-8 text for the id in the current language. Never returns an empty view for a valid id.
[[nodiscard]] std::string_view text(TextId id) noexcept;

/// Short alias for UI code: ImGui::Button(tr(TextId::Close))
[[nodiscard]] inline const char* tr(TextId id) noexcept { return text(id).data(); }

} // namespace rc::i18n
