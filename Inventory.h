#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Item.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Inventory.generated.h"

// Drag state is responsible for tracking all the stages of drag interations
enum class EDragState : uint8
{
    None,     // Nothing is being dragged
    Pressed,  // Item has been slected
    Dragging, // Item is being dragged
    Dropped   // Item has been dropped
};

// Inventory user widget calls (Main class)
UCLASS()
class UInventory : public UUserWidget
{
    GENERATED_BODY()

public:
    UInventory(const FObjectInitializer& ObjectInitializer);

    // Called when the widget is first initialized
    virtual void NativeOnInitialized() override;

    // Called when the widget is constructed or reconstructed
    virtual void NativeConstruct() override;

    // ******************** Mouse events for drag detection ********************

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // ******************** Open and close for toggling Inventory via Tab ********************

    UFUNCTION()
    void Open();

    UFUNCTION()
    void Close();

    // ***************************************************************************************

    // Adds an item to the inventory
    UFUNCTION()
    void AddItem(AActor* ItemActor);

    // Checks if the inventory is full
    UFUNCTION()
    bool IsInventoryFull() const;

    // Returns a reference to the item array
    UFUNCTION()
    const TArray<FItem>& GetItems() const;

    // Returns the array of inventory slots
    TArray<TObjectPtr<UBorder>> GetSlots() const;

    // Returns the grid widget containing all slot data
    TObjectPtr<UUniformGridPanel> GetGrid() const;

private:

    // ************* Max rows and columns for determening grid size *************
    UPROPERTY()
    uint32 MaxRows;

    UPROPERTY()
    uint32 MaxColumns;

    // **************************************************************************

    UPROPERTY()
    TArray<FItem> Items;

    UPROPERTY()
    TArray<TObjectPtr<UBorder>> Slots;

    UPROPERTY()
    TObjectPtr<UCanvasPanel> Canvas;

    // Inventory's grey background
    UPROPERTY()
    TObjectPtr<UBorder> Background;

    UPROPERTY()
    TObjectPtr<UCanvasPanelSlot> BackgroundSlot;

    UPROPERTY()
    TObjectPtr<UVerticalBox> BackgroundVerticalBox;

    // Inventory Title set to "Inventory"
    UPROPERTY()
    TObjectPtr<UTextBlock> Title;

    UPROPERTY()
    TObjectPtr<UVerticalBoxSlot> TitleVerticalBoxSlot;

    // Grid of the inventory slots
    UPROPERTY()
    TObjectPtr<UUniformGridPanel> Grid;

    UPROPERTY()
    TObjectPtr<UVerticalBoxSlot> GridVerticalBoxSlot;

    UPROPERTY()
    TObjectPtr<UUniformGridSlot> GridSlot;

    // Visual Representation of PoppedOutItem
    UPROPERTY()
    TObjectPtr<UOverlay> PoppedOutItemWidget;

    // Index of the currently hovered slot
    UPROPERTY()
    int32 HoveredSlotIndex;

    // Index of the slot where the drag used to be originaly 
    UPROPERTY()
    int32 OriginSlotIndex;

    // PoppedOutItem used to copy internal item when item outside inventoty 
    UPROPERTY()
    FItem PoppedOutItem;

    // Mouse position in screen space
    UPROPERTY()
    FVector2D MouseScreenSpacePosition;

    // Mouse position relative to the widget
    UPROPERTY()
    FVector2D MouseWidgetLocalPosition;

    // Current drag state
    EDragState DragState;

    UPROPERTY()
    bool bIsMouseInsideInventory;

private:

    // Constructs the initial layout and slots
    UFUNCTION()
    void Create();

    // Updates all slot visuals based on current item data
    UFUNCTION()
    void RefreshInventory();

    // Creates or updates the icon for a single item slot
    UFUNCTION()
    void CreateItemIcon(uint32 SlotIndex);

    // Finds the first empty inventory slot index
    UFUNCTION()
    int32 FindFirstEmptySlot() const;

    // Resposible for updating all items position on drag
    UFUNCTION()
    void InternallyRearrangeItems(const FPointerEvent& MouseEvent);

    // Returns the index of the hovered slot under the mouse
    UFUNCTION()
    int32 FindHoveredSlot(const FPointerEvent& InMouseEvent);
};
