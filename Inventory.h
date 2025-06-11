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
UENUM(BlueprintType)
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

    // NativeOnMouseButtonDown used for storing item index, updating dragging states and store mouse position (Helpefull for later "Sorting...")
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // Add Item Method
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddItem(AActor* ItemActor);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RemoveItem(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void Open();

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void Close();

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool GetIsInventoryFull() const;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    const TArray<FItem>& GetItems() const;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    TArray<UBorder*> GetForegroundBorders() const;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    UUniformGridPanel* GetGrid() const;

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
    TObjectPtr<UCanvasPanelSlot> BackgroundBorder_S;

    UPROPERTY()
    TObjectPtr<UVerticalBox> Background_VB;

    UPROPERTY()
    TObjectPtr<UTextBlock> Title;

    UPROPERTY()
    TObjectPtr<UVerticalBoxSlot> Title_VBS;

    UPROPERTY()
    TObjectPtr<UUniformGridPanel> Grid;

    UPROPERTY()
    TObjectPtr<UVerticalBoxSlot> Grid_VBS;

    UPROPERTY()
    TObjectPtr<UUniformGridSlot> Grid_S;

    UPROPERTY()
    TObjectPtr<UOverlay> DraggedItemWidget;

    static uint32 ItemCounter;

    UPROPERTY()
    bool bPendingRemoval;

    UPROPERTY()
    int32 DraggedItemIndex;

    UPROPERTY()
    int32 OriginalSlotIndex;

    UPROPERTY()
    int32 PreviousSlotIndex;

    UPROPERTY()
    FItem DraggedItem;

    UPROPERTY()
    bool bIsItemDragging;

    UPROPERTY()
    bool bHasItemDragStarted;

    UPROPERTY()
    FVector2D MousePosition;

    UPROPERTY()
    bool bIsSliding;

    UPROPERTY()
    int32 SlideFromIndex;

    UPROPERTY()
    int32 SlideToIndex;

    UPROPERTY()
    float SlideProgress;

    UPROPERTY()
    float SlideDuration;

    UPROPERTY()
    FItem SlidingItem;

    UPROPERTY()
    int32 ScheduledFromIndex;

    UPROPERTY()
    int32 ScheduledToIndex;

    UPROPERTY()
    EDirection ScheduledDirection;

    EDragState DragState;

private:

    void Create();
    void UpdateSlotUI(uint32 SlotIndex);
    void RemoveItemIcon(uint32 SlotIndex);
    void CreateItemIcon(uint32 SlotIndex);
    void CreateIconCounterText(uint32 SlotIndex);
    uint32 FindFirstEmptySlot() const;
    EDirection GetMoveDirection(uint32 RowA, uint32 ColA, uint32 RowB, uint32 ColB) const;
    FVector2D GetSlotPosition(uint32 SlotIndex) const;

    void MoveItem(const FPointerEvent& MouseEvent);

    uint32 FindHoveredItemIndex(const FPointerEvent& InMouseEvent);

    void RefreshInventoryUI();
};
