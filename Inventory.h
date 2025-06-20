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

// Direction enum created for setting Item movement
enum class EDirection : uint8
{
    Null,
    Up,
    Down,
    Left,
    Right
};

enum class EDragState : uint8
{
    Null,
    Select,
    Moved,
    Released
};

UCLASS()
class UInventory : public UUserWidget
{
    GENERATED_BODY()

public:
    UInventory(const FObjectInitializer& ObjectInitializer);

    // NativeOnInitialized used for creating and set up inventory's UI
    virtual void NativeOnInitialized() override;

    // NativeNativeConstruct used for reconstructing inventory Widgets 
    virtual void NativeConstruct() override;

    // NativeOnMouseButtonDown used for keeping track of the hovered slot, mouse position, drag state and dragged item 
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;


    void Open();

    void Close();

    void AddItem(AActor* ItemActor);

    bool GetIsInventoryFull() const;

    const TArray<FItem>& GetItems() const;

    TArray<TObjectPtr<UBorder>>  GetForegroundBorders() const;

    TObjectPtr<UUniformGridPanel> GetGrid() const;

private:

    uint32 MaxRows;

    uint32 MaxColumns;

    UPROPERTY()
    bool bIsInventoryFull;

    UPROPERTY()
    TArray<FItem> Items;

    UPROPERTY()
    TArray<TObjectPtr<UBorder>> ForegroundBorders;

    UPROPERTY()
    TObjectPtr<UCanvasPanel> Canvas;

    UPROPERTY()
    TObjectPtr<UBorder> BackgroundBorder;

    UPROPERTY()
    TObjectPtr<UCanvasPanelSlot> BackgroundBorderSlot;

    UPROPERTY()
    TObjectPtr<UVerticalBox> BackgroundVerticalBox;

    UPROPERTY()
    TObjectPtr<UTextBlock> Title;

    UPROPERTY()
    TObjectPtr<UVerticalBoxSlot> TitleVerticalBoxSlot;

    UPROPERTY()
    TObjectPtr<UUniformGridPanel> Grid;

    UPROPERTY()
    TObjectPtr<UVerticalBoxSlot> GridVerticalBoxSlot;

    UPROPERTY()
    TObjectPtr<UUniformGridSlot> GridSlot;

    UPROPERTY()
    TObjectPtr<UOverlay> DraggedItemWidget;

    static uint32 ItemCounter;

    UPROPERTY()
    int32 DragStartSlot;

    UPROPERTY()
    int32 OriginalSlot;

    UPROPERTY()
    int32 PreviousSlotIndex;

    UPROPERTY()
    FItem DraggedItem;

    UPROPERTY()
    FVector2D MousePosition;

    EDragState DragState;

private:

    void Create();

    void UpdateSlotUI(uint32 SlotIndex);

    void RemoveItemIcon(uint32 SlotIndex);

    void CreateItemIcon(uint32 SlotIndex);

    uint32 FindFirstEmptySlot() const;

    EDirection GetMoveDirection(uint32 RowA, uint32 ColA, uint32 RowB, uint32 ColB) const;

    FVector2D GetSlotPosition(uint32 SlotIndex) const;

    void MoveItem(const FPointerEvent& MouseEvent);

    void RemoveItem(int32 SlotIndex);

    uint32 FindHoveredSlot(const FPointerEvent& InMouseEvent);

    void RefreshInventoryUI();
};
