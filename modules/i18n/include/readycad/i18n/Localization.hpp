#pragma once

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace rc::i18n {

/// Every user-visible string has an id. Code is written in English; the program speaks Turkish,
/// Russian or English.
///
/// Why an enum instead of string keys ("menu.file"): a typo becomes a compile error, and a test
/// checks that every id has a translation in every language. Adding a language = adding one more
/// table.
///
/// Two kinds of texts need more than a lookup:
///  * a phrase with values in it is a template with "{}" places (format()): each language puts
///    them where its grammar wants them ("G1'den 300", "300 от Ш1", "300 from W1");
///  * a word after a number lists its forms separated by '|' (plural()): Turkish keeps one form
///    ("levha"), English two ("sheet|sheets"), Russian three ("лист|листа|листов").
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
    ConsumeTitle, ConsumeBody, ConsumeDone, CacheMemory, PartsOnSheet, UseRemnants,
    UseRemnantsHint, RemnantsNotUsed,
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
    // Corner kitchens: runs on side walls, corners, blind corner cabinets
    KindKitchenRun, KindKitchenCorner, KindBlindPanel, CatalogKitchenL, CatalogKitchenU, RunBack,
    RunLeft, RunRight, CornerLeft, CornerRight, BlindBaseCabinet, BlindWallCabinet,
    BackWallLength, Filler, CornerBaseWidth, CornerBaseFiller, CornerWallWidth, CornerWallFiller,
    PagePlan, ViewPlan, WorktopJoint, LongWorktopWarning, WorktopMaxLength,
    // Languages, file dialogs, package names (ASCII), counted words, drilling warnings
    MenuLanguage, DocumentLanguage, SameAsProgram, FilterProject, FilterProfile, FilterPdf,
    FilterDxf, FilterCsv, FolderProduction, FolderCutting, FileSummary, FileCutList, SheetsCount,
    WarnNoConnectorPlace, WarnNoShelfWall, WarnNoHingeWall, WarnUnknownHandle,
    WarnHandleDoesNotFit, UnitMm, UnitM, PercentValue,
    // L corner cabinets
    KindCornerCabinet, KindFoldingDoor, LBaseCabinet, LWallCabinet, CornerTypeLabel, CornerBlind,
    CornerLShaped, LegAlongBack, LegAlongSide, BaseLegs, WallLegs, HwFoldingHinge,
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

/// The languages of the program: of its interface and of its documents.
enum class Language { Turkish, Russian, English };

/// Every language, in the order the menu offers them.
[[nodiscard]] std::span<const Language> languages() noexcept;
/// "tr", "ru", "en": how the settings file and the command line name it.
[[nodiscard]] std::string_view languageCode(Language language) noexcept;
[[nodiscard]] std::optional<Language> languageFromCode(std::string_view code) noexcept;
/// The language's own name, whatever the current language: "Türkçe", "Русский", "English".
[[nodiscard]] std::string_view languageName(Language language) noexcept;

void setLanguage(Language language) noexcept;
[[nodiscard]] Language currentLanguage() noexcept;

/// Another language for a while, then back: the documents of a package in a language of their
/// own while the program keeps its. For one thread at a time (the documents are written on the
/// thread that draws the window).
class LanguageScope {
public:
    explicit LanguageScope(Language language) noexcept : previous_(currentLanguage())
    {
        setLanguage(language);
    }
    ~LanguageScope() { setLanguage(previous_); }
    LanguageScope(const LanguageScope&) = delete;
    LanguageScope& operator=(const LanguageScope&) = delete;

private:
    Language previous_;
};

/// UTF-8 text for the id in the current language. Never returns an empty view for a valid id.
[[nodiscard]] std::string_view text(TextId id) noexcept;

/// Short alias for UI code: ImGui::Button(tr(TextId::Close))
[[nodiscard]] inline const char* tr(TextId id) noexcept { return text(id).data(); }

/// The template of `id` with its places filled: "{}" takes the next value, "{1}".."{9}" the
/// value at that place (a language may put them in another order).
[[nodiscard]] std::string format(TextId id, std::initializer_list<std::string_view> values);

/// The form of a counted word (its forms separated by '|') that goes with `count`.
[[nodiscard]] std::string_view plural(TextId id, long long count) noexcept;
/// The count and its word: "3 листа", "1 sheet", "2 levha".
[[nodiscard]] std::string countText(long long count, TextId id);

} // namespace rc::i18n
