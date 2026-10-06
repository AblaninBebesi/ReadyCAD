#pragma once

#include <cstddef>
#include <string_view>

namespace rc::ui {

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

    Count // keep last
};

enum class Language { Turkish };

void setLanguage(Language language) noexcept;
[[nodiscard]] Language currentLanguage() noexcept;

/// UTF-8 text for the id in the current language. Never returns an empty view for a valid id.
[[nodiscard]] std::string_view text(TextId id) noexcept;

/// Short alias for UI code: ImGui::Button(tr(TextId::Close))
[[nodiscard]] inline const char* tr(TextId id) noexcept { return text(id).data(); }

} // namespace rc::ui
