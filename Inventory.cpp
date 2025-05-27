#include "Inventory.h"

uint32 UInventory::ItemCounter = 0;

UInventory::UInventory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
 ,MaxRows(3)
 , MaxColumns(4)
 , bIsInventoryFull(false)
 , DraggedItemWidget(nullptr)
 , bPendingRemoval(false)
 , DraggedItemIndex(INDEX_NONE)
 , OriginalSlotIndex(INDEX_NONE)
 , PreviousSlotIndex(INDEX_NONE)
 , bIsDragging(false)
 , bDragStarted(false)
 , DragStartPosition(FVector2D::ZeroVector)
 , bIsSliding(false)
 , SlideFromIndex(INDEX_NONE)
 , SlideToIndex(INDEX_NONE)
 , SlideProgress(0.0f)
 , SlideDuration(0.2f)
 , bAnimationScheduled(false)
 , ScheduledFromIndex(INDEX_NONE)
 , ScheduledToIndex(INDEX_NONE)
 , ScheduledDirection(EDirection::None)
 , MoveCount(0)
{
    Items.SetNum(MaxRows * MaxColumns);
    ForegroundBorders.SetNum(MaxRows * MaxColumns);
    IconSlots.SetNum(MaxRows * MaxColumns);
    bCounterTextUpdated.SetNum(MaxRows * MaxColumns);
}

void UInventory::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    bIsInventoryFull = false;
    DraggedItemIndex = INDEX_NONE;
    OriginalSlotIndex = INDEX_NONE;
    PreviousSlotIndex = INDEX_NONE;
    bIsDragging = false;
    bIsSliding = false;
    SlideFromIndex = INDEX_NONE;
    SlideToIndex = INDEX_NONE;
    SlideProgress = 0.0f;
    bAnimationScheduled = false;
    ScheduledFromIndex = INDEX_NONE;
    ScheduledToIndex = INDEX_NONE;
    ScheduledDirection = EDirection::None;
    MoveCount = 0;
    bDragStarted = false;
    DragStartPosition = FVector2D::ZeroVector;

    if (!WidgetTree)
    {
        UE_LOG(LogTemp, Warning, TEXT("WidgetTree is invalid"));
        return;
    }

    Canvas = NewObject<UCanvasPanel>(this);
    WidgetTree->RootWidget = Canvas;

    BackgroundBorder = NewObject<UBorder>(this);
    BackgroundBorder->SetBrushColor(FLinearColor::Gray);

    Title = NewObject<UTextBlock>(this);
    Title->SetText(FText::FromString(TEXT("Inventory")));

    UVerticalBox* ContentBox = NewObject<UVerticalBox>(this);
    UVerticalBoxSlot* TitleBoxSlot = ContentBox->AddChildToVerticalBox(Title);
    TitleBoxSlot->SetHorizontalAlignment(HAlign_Center);
    TitleBoxSlot->SetVerticalAlignment(VAlign_Top);
    TitleBoxSlot->SetPadding(FMargin(10, 10, 10, 10));

    Grid = NewObject<UUniformGridPanel>(this);
    Grid->SetSlotPadding(FMargin(10, 10, 10, 10));
    UVerticalBoxSlot* GridBoxSlot = ContentBox->AddChildToVerticalBox(Grid);
    GridBoxSlot->SetHorizontalAlignment(HAlign_Fill);
    GridBoxSlot->SetVerticalAlignment(VAlign_Fill);
    GridBoxSlot->SetPadding(FMargin(10, 10, 10, 10));

    BackgroundBorder->SetContent(ContentBox);
    BackgroundBorderSlot = Canvas->AddChildToCanvas(BackgroundBorder);
    BackgroundBorderSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
    BackgroundBorderSlot->SetAlignment(FVector2D(0.5f, 0.5f));

    // Set fixed background size to match the provided snippet
    BackgroundBorderSlot->SetOffsets(FMargin(0, -100, 510.0f, 500.0f));

    SetRenderScale(FVector2D(1.0f, 1.0f));
    Create();

    if (GEngine && GEngine->GameViewport)
    {
        FVector2D ViewportSize;
        GEngine->GameViewport->GetViewportSize(ViewportSize);
        UE_LOG(LogTemp, Log, TEXT("Viewport Size: %s"), *ViewportSize.ToString());

        if (BackgroundBorder)
        {
            FGeometry BorderGeometry = BackgroundBorder->GetCachedGeometry();
            FVector2D BorderTopLeft = BorderGeometry.LocalToAbsolute(FVector2D::ZeroVector);
            FVector2D BorderSize = BorderGeometry.GetLocalSize();
            UE_LOG(LogTemp, Log, TEXT("BackgroundBorder: TopLeft=%s, Size=%s"), *BorderTopLeft.ToString(), *BorderSize.ToString());
        }
    }
}

void UInventory::NativeConstruct()
{
    Super::NativeConstruct();

    if (BackgroundBorder) BackgroundBorder->ForceLayoutPrepass();
    if (Grid) Grid->ForceLayoutPrepass();
    if (Canvas) Canvas->ForceLayoutPrepass();
    for (TObjectPtr<UBorder> Border : ForegroundBorders)
    {
        if (Border) Border->ForceLayoutPrepass();
    }
}

void UInventory::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bAnimationScheduled)
    {
        bool bGeometryReady = true;
        for (int32 i = 0; i < ForegroundBorders.Num(); ++i)
        {
            FVector2D Position = GetSlotPosition(i);
            FGeometry SlotGeometry = ForegroundBorders[i]->GetCachedGeometry();
            FVector2D SlotSize = SlotGeometry.GetLocalSize();
            if (SlotSize.X < 10.0f || SlotSize.Y < 10.0f)
            {
                UE_LOG(LogTemp, Warning, TEXT("NativeTick: Slot %d geometry invalid: Position=%s, Size=%s"),
                    i, *Position.ToString(), *SlotSize.ToString());
                bGeometryReady = false;
                break;
            }
        }

        if (bGeometryReady)
        {
            StartSlideAnimation(ScheduledFromIndex, ScheduledToIndex, ScheduledDirection);
            bAnimationScheduled = false;
            ScheduledFromIndex = INDEX_NONE;
            ScheduledToIndex = INDEX_NONE;
            ScheduledDirection = EDirection::None;
        }
        else
        {
            if (Canvas) Canvas->ForceLayoutPrepass();
            if (Grid) Grid->ForceLayoutPrepass();
            for (TObjectPtr<UBorder> Border : ForegroundBorders)
            {
                if (Border) Border->ForceLayoutPrepass();
            }
        }
    }

    if (bIsSliding)
    {
        SlideProgress += InDeltaTime / SlideDuration;
        if (SlideProgress >= 1.0f)
        {
            bIsSliding = false;
            SlideProgress = 0.0f;
            SlideFromIndex = INDEX_NONE;
            SlideToIndex = INDEX_NONE;
            SlidingItem = FItem();
            SlidingOverlays.Empty();
            SlideFromIndices.Empty();
            SlideToIndices.Empty();
            SlidingItems.Empty();

            if (Grid) Grid->ForceLayoutPrepass();
            if (Canvas) Canvas->ForceLayoutPrepass();
        }
    }
}

FReply UInventory::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        uint64 HoveredIndex = FindHoveredItemIndex(InMouseEvent);
        if (HoveredIndex != INDEX_NONE && Items.IsValidIndex(HoveredIndex) && Items[HoveredIndex].ReferencedActorClass)
        {
            DraggedItemIndex = HoveredIndex;
            OriginalSlotIndex = HoveredIndex;
            DraggedItem = Items[HoveredIndex];
            bIsDragging = true;
            bDragStarted = false;
            DragStartPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

            if (ForegroundBorders.IsValidIndex(DraggedItemIndex))
            {
                ForegroundBorders[DraggedItemIndex]->SetBrushColor(FLinearColor(0.9f, 0.0f, 0.9f, 1.0f));
            }

            UE_LOG(LogTemp, Log, TEXT("NativeOnMouseButtonDown: Started drag potential for item %d from slot %d"),
                DraggedItem.Index, HoveredIndex);
            return FReply::Handled();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("NativeOnMouseButtonDown: No valid item at HoveredIndex=%d"), HoveredIndex);
        }
    }
    return FReply::Unhandled();
}

FReply UInventory::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsDragging)
    {
        FVector2D CurrentPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        UE_LOG(LogTemp, Log, TEXT("NativeOnMouseMove: Dragging item %d, MousePos=%s, bDragStarted=%s"),
            DraggedItem.Index, *CurrentPosition.ToString(), bDragStarted ? TEXT("True") : TEXT("False"));

        if (bDragStarted && ForegroundBorders.IsValidIndex(DraggedItemIndex))
        {
            ForegroundBorders[DraggedItemIndex]->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f));
        }

        if (!bDragStarted)
        {
            if (BackgroundBorder) BackgroundBorder->ForceLayoutPrepass();
            if (Canvas) Canvas->ForceLayoutPrepass();
            if (Grid) Grid->ForceLayoutPrepass();
            for (TObjectPtr<UBorder> Border : ForegroundBorders)
            {
                if (Border) Border->ForceLayoutPrepass();
            }

            bool bIsOutside = false;
            FVector2D MouseAbsPos = InMouseEvent.GetScreenSpacePosition();

            if (ForegroundBorders.IsValidIndex(DraggedItemIndex) && ForegroundBorders[DraggedItemIndex])
            {
                FGeometry SlotGeometry = ForegroundBorders[DraggedItemIndex]->GetCachedGeometry();
                FVector2D SlotAbsTopLeft = SlotGeometry.LocalToAbsolute(FVector2D::ZeroVector);
                FVector2D SlotAbsSize = SlotGeometry.GetLocalSize();
                FVector2D SlotAbsBottomRight = SlotAbsTopLeft + SlotAbsSize;

                if (SlotAbsSize.X < 10.0f || SlotAbsSize.Y < 10.0f)
                {
                    UE_LOG(LogTemp, Warning, TEXT("NativeOnMouseMove: Slot %d geometry invalid: TopLeft=%s, Size=%s. Skipping edge check."),
                        DraggedItemIndex, *SlotAbsTopLeft.ToString(), *SlotAbsSize.ToString());
                    return FReply::Handled();
                }

                int32 Row = DraggedItemIndex / MaxColumns;
                int32 Col = DraggedItemIndex % MaxColumns;

                float Padding = 1.0f;
                float Buffer = 1.0f;
                float Distance = 0.0f;

                UE_LOG(LogTemp, Log, TEXT("NativeOnMouseMove: Slot %d Edge Check - TopLeft=%s, BottomRight=%s, MousePos=%s, Padding=%f, Buffer=%f"),
                    DraggedItemIndex, *SlotAbsTopLeft.ToString(), *SlotAbsBottomRight.ToString(), *MouseAbsPos.ToString(), Padding, Buffer);

                if (Row == 0 && MouseAbsPos.Y < SlotAbsTopLeft.Y - Padding - Buffer)
                {
                    bIsOutside = true;
                    Distance = SlotAbsTopLeft.Y - MouseAbsPos.Y;
                    UE_LOG(LogTemp, Log, TEXT("Top Edge Trigger: Slot=%d, MousePos.Y=%f, SlotTop=%f, Distance=%f"),
                        DraggedItemIndex, MouseAbsPos.Y, SlotAbsTopLeft.Y, Distance);
                }
                else if (Row == MaxRows - 1 && MouseAbsPos.Y > SlotAbsBottomRight.Y + Padding + Buffer)
                {
                    bIsOutside = true;
                    Distance = MouseAbsPos.Y - SlotAbsBottomRight.Y;
                    UE_LOG(LogTemp, Log, TEXT("Bottom Edge Trigger: Slot=%d, MousePos.Y=%f, SlotBottom=%f, Distance=%f"),
                        DraggedItemIndex, MouseAbsPos.Y, SlotAbsBottomRight.Y, Distance);
                }
                else if (Col == 0 && MouseAbsPos.X < SlotAbsTopLeft.X - Padding - Buffer)
                {
                    bIsOutside = true;
                    Distance = SlotAbsTopLeft.X - MouseAbsPos.X;
                    UE_LOG(LogTemp, Log, TEXT("Left Edge Trigger: Slot=%d, MousePos.X=%f, SlotLeft=%f, Distance=%f"),
                        DraggedItemIndex, MouseAbsPos.X, SlotAbsTopLeft.X, Distance);
                }
                else if (Col == MaxColumns - 1 && MouseAbsPos.X > SlotAbsBottomRight.X + Padding + Buffer)
                {
                    bIsOutside = true;
                    Distance = MouseAbsPos.X - SlotAbsBottomRight.X;
                    UE_LOG(LogTemp, Log, TEXT("Right Edge Trigger: Slot=%d, MousePos.X=%f, SlotRight=%f, Distance=%f"),
                        DraggedItemIndex, MouseAbsPos.X, SlotAbsBottomRight.X, Distance);
                }

                if (bIsOutside)
                {
                    bDragStarted = true;

                    if (Canvas)
                    {
                        DraggedItemWidget = NewObject<UOverlay>(this);
                        DraggedItemWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

                        UImage* ItemImage = NewObject<UImage>(this);
                        ItemImage->SetVisibility(ESlateVisibility::Visible);
                        if (DraggedItem.IconTexture.IsValid())
                        {
                            ItemImage->SetBrushFromTexture(DraggedItem.IconTexture.Get());
                        }
                        else
                        {
                            ItemImage->SetColorAndOpacity(FLinearColor::Blue);
                        }
                        UOverlaySlot* ImageSlot = DraggedItemWidget->AddChildToOverlay(ItemImage);
                        ImageSlot->SetHorizontalAlignment(HAlign_Fill);
                        ImageSlot->SetVerticalAlignment(VAlign_Fill);

                        UTextBlock* ItemCounterText = NewObject<UTextBlock>(this);
                        ItemCounterText->SetVisibility(ESlateVisibility::Visible);
                        ItemCounterText->SetText(FText::AsNumber(DraggedItem.Index));
                        ItemCounterText->SetColorAndOpacity(FLinearColor::Red);
                        ItemCounterText->SetJustification(ETextJustify::Center);
                        ItemCounterText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 20));
                        UOverlaySlot* TextOverlaySlot = DraggedItemWidget->AddChildToOverlay(ItemCounterText);
                        TextOverlaySlot->SetHorizontalAlignment(HAlign_Center);
                        TextOverlaySlot->SetVerticalAlignment(VAlign_Center);

                        UCanvasPanelSlot* WidgetSlot = Canvas->AddChildToCanvas(DraggedItemWidget);
                        WidgetSlot->SetSize(FVector2D(100.0f, 100.0f));
                        WidgetSlot->SetPosition(CurrentPosition - FVector2D(50.0f, 50.0f));
                        WidgetSlot->SetZOrder(100);
                    }

                    UE_LOG(LogTemp, Log, TEXT("Drag started for item %d from edge slot %d"), DraggedItem.Index, DraggedItemIndex);
                }
            }
        }

        if (bDragStarted && DraggedItemWidget && Canvas)
        {
            if (UCanvasPanelSlot* WidgetSlot = Cast<UCanvasPanelSlot>(DraggedItemWidget->Slot))
            {
                WidgetSlot->SetPosition(CurrentPosition - FVector2D(50.0f, 50.0f));
            }
        }

        MoveItem(InMouseEvent, false, false);
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UInventory::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsDragging)
    {
        uint64 HoveredIndex = FindHoveredItemIndex(InMouseEvent);
        bool bValidDrop = (HoveredIndex != INDEX_NONE && Items.IsValidIndex(HoveredIndex));
        UE_LOG(LogTemp, Log, TEXT("NativeOnMouseButtonUp: HoveredIndex=%d, ValidDrop=%s"),
            HoveredIndex, bValidDrop ? TEXT("True") : TEXT("False"));

        if (ForegroundBorders.IsValidIndex(OriginalSlotIndex))
        {
            ForegroundBorders[OriginalSlotIndex]->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f));
        }
        if (ForegroundBorders.IsValidIndex(DraggedItemIndex))
        {
            ForegroundBorders[DraggedItemIndex]->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f));
        }
        if (PreviousSlotIndex != INDEX_NONE && ForegroundBorders.IsValidIndex(PreviousSlotIndex))
        {
            ForegroundBorders[PreviousSlotIndex]->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f));
        }

        if (DraggedItemWidget && Canvas)
        {
            Canvas->RemoveChild(DraggedItemWidget);
            DraggedItemWidget = nullptr;
        }

        if (bValidDrop && HoveredIndex != OriginalSlotIndex)
        {
            if (Items[HoveredIndex].ReferencedActorClass)
            {
                FItem TempItem = Items[HoveredIndex];
                Items[HoveredIndex] = DraggedItem;
                Items[OriginalSlotIndex] = TempItem;
                UpdateSlotUI(OriginalSlotIndex);
                UpdateSlotUI(HoveredIndex);
                UE_LOG(LogTemp, Log, TEXT("Swapped item %d with item in slot %d"), DraggedItem.Index, HoveredIndex);
            }
            else
            {
                Items[HoveredIndex] = DraggedItem;
                Items[OriginalSlotIndex] = FItem();
                UpdateSlotUI(OriginalSlotIndex);
                UpdateSlotUI(HoveredIndex);
                UE_LOG(LogTemp, Log, TEXT("Moved item %d to empty slot %d"), DraggedItem.Index, HoveredIndex);
            }
        }
        else if (bValidDrop && HoveredIndex == OriginalSlotIndex)
        {
            UE_LOG(LogTemp, Log, TEXT("Item %d dropped back to original slot %d"), DraggedItem.Index, HoveredIndex);
        }
        else
        {
            RemoveItem(OriginalSlotIndex);
            UE_LOG(LogTemp, Log, TEXT("Item %d dropped outside inventory, removed"), DraggedItem.Index);
        }

        bIsDragging = false;
        bDragStarted = false;
        DraggedItemIndex = INDEX_NONE;
        PreviousSlotIndex = INDEX_NONE;
        OriginalSlotIndex = INDEX_NONE;
        DraggedItem = FItem();
        DragStartPosition = FVector2D::ZeroVector;

        if (Grid) Grid->ForceLayoutPrepass();
        if (Canvas) Canvas->ForceLayoutPrepass();
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

void UInventory::AddItem(AActor* ItemActor)
{
    if (!ItemActor) return;

    if (ItemCounter >= static_cast<uint64>(MaxRows * MaxColumns))
    {
        bIsInventoryFull = true;
        return;
    }

    uint64 EmptySlotIndex = FindFirstEmptySlot();
    if (EmptySlotIndex == INDEX_NONE)
    {
        bIsInventoryFull = true;
        return;
    }

    Items[EmptySlotIndex] = FItem();
    Items[EmptySlotIndex].ReferencedActorClass = ItemActor->GetClass();
    Items[EmptySlotIndex].WorldTransform = ItemActor->GetActorTransform();
    Items[EmptySlotIndex].Index = ItemCounter;

    UpdateSlotUI(EmptySlotIndex);
    ItemActor->Destroy();
    ItemCounter++;
    bIsInventoryFull = (FindFirstEmptySlot() == INDEX_NONE);
}

void UInventory::RemoveItem(int32 SlotIndex)
{
    if (!Items.IsValidIndex(SlotIndex) || !Items[SlotIndex].ReferencedActorClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("RemoveItem: Invalid slot %d or no item to remove"), SlotIndex);
        return;
    }

    Items[SlotIndex] = FItem();
    RemoveItemIcon(SlotIndex);
    if (ItemCounter > 0) ItemCounter--;

    uint64 NewIndex = 0;
    for (uint64 i = 0; i < static_cast<uint64>(Items.Num()); ++i)
    {
        if (Items[i].ReferencedActorClass)
        {
            Items[i].Index = NewIndex++;
            CreateIconCounterText(i);
        }
    }
    ItemCounter = NewIndex;
    bIsInventoryFull = (FindFirstEmptySlot() == INDEX_NONE);
}

uint32 UInventory::FindHoveredItemIndex(const FPointerEvent& InMouseEvent) const
{
    if (!Grid || ForegroundBorders.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("FindHoveredItemIndex: Grid or ForegroundBorders invalid"));
        return INDEX_NONE;
    }

    if (Canvas) Canvas->ForceLayoutPrepass();
    if (Grid) Grid->ForceLayoutPrepass();
    for (TObjectPtr<UBorder> Border : ForegroundBorders)
    {
        if (Border) Border->ForceLayoutPrepass();
    }

    FVector2D MousePos = InMouseEvent.GetScreenSpacePosition();
    uint64 ClosestIndex = INDEX_NONE;
    float MinDistance = FLT_MAX;

    bool bAnyValidGeometry = false;
    for (uint64 Row = 0; Row < MaxRows; Row++)
    {
        for (uint64 Col = 0; Col < MaxColumns; Col++)
        {
            uint64 Index = Row * MaxColumns + Col;
            if (!ForegroundBorders.IsValidIndex(Index) || !ForegroundBorders[Index])
            {
                UE_LOG(LogTemp, Warning, TEXT("FindHoveredItemIndex: Slot %d invalid or null"), Index);
                continue;
            }

            FGeometry SlotGeometry = ForegroundBorders[Index]->GetCachedGeometry();
            FVector2D SlotAbsTopLeft = SlotGeometry.LocalToAbsolute(FVector2D::ZeroVector);
            FVector2D SlotAbsSize = SlotGeometry.GetLocalSize();
            FVector2D SlotAbsBottomRight = SlotAbsTopLeft + SlotAbsSize;

            UE_LOG(LogTemp, Log, TEXT("FindHoveredItemIndex: Slot %d, TopLeft=%s, Size=%s, BottomRight=%s, MousePos=%s"),
                Index, *SlotAbsTopLeft.ToString(), *SlotAbsSize.ToString(), *SlotAbsBottomRight.ToString(), *MousePos.ToString());

            if (SlotAbsSize.X < 10.0f || SlotAbsSize.Y < 10.0f)
            {
                UE_LOG(LogTemp, Warning, TEXT("FindHoveredItemIndex: Slot %d geometry too small: Size=%s"),
                    Index, *SlotAbsSize.ToString());
                continue;
            }

            bAnyValidGeometry = true;

            if (MousePos.X >= SlotAbsTopLeft.X && MousePos.X <= SlotAbsBottomRight.X &&
                MousePos.Y >= SlotAbsTopLeft.Y && MousePos.Y <= SlotAbsBottomRight.Y)
            {
                FVector2D SlotCenter = SlotAbsTopLeft + (SlotAbsSize / 2.0f);
                float Distance = FVector2D::Distance(MousePos, SlotCenter);
                if (Distance < MinDistance)
                {
                    MinDistance = Distance;
                    ClosestIndex = Index;
                    UE_LOG(LogTemp, Log, TEXT("FindHoveredItemIndex: Slot %d hit, Distance=%f"), Index, Distance);
                }
            }
        }
    }

    if (ClosestIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning, TEXT("FindHoveredItemIndex: No valid slot hit. AnyValidGeometry=%s, MousePos=%s"),
            bAnyValidGeometry ? TEXT("True") : TEXT("False"), *MousePos.ToString());
    }

    return ClosestIndex;
}

void UInventory::MoveItem(const FPointerEvent& MouseEvent, bool bItemMovementStarted, bool bItemMovementFinished)
{
    if (!bIsDragging)
    {
        UE_LOG(LogTemp, Warning, TEXT("MoveItem: Not dragging, exiting"));
        return;
    }

    uint64 HoveredIndex = FindHoveredItemIndex(MouseEvent);
    UE_LOG(LogTemp, Log, TEXT("MoveItem: HoveredIndex=%d, OriginalSlotIndex=%d, DraggedItemIndex=%d"),
        HoveredIndex, OriginalSlotIndex, DraggedItemIndex);

    if (HoveredIndex == INDEX_NONE || !Items.IsValidIndex(HoveredIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("MoveItem: Invalid HoveredIndex=%d or out of bounds"), HoveredIndex);
        return;
    }

    if (PreviousSlotIndex != INDEX_NONE && PreviousSlotIndex != HoveredIndex && ForegroundBorders.IsValidIndex(PreviousSlotIndex))
    {
        ForegroundBorders[PreviousSlotIndex]->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f));
    }

    if (ForegroundBorders.IsValidIndex(HoveredIndex))
    {
        ForegroundBorders[HoveredIndex]->SetBrushColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
    }

    PreviousSlotIndex = HoveredIndex;

    if (HoveredIndex == OriginalSlotIndex)
    {
        UE_LOG(LogTemp, Log, TEXT("MoveItem: HoveredIndex same as OriginalSlotIndex, skipping"));
        return;
    }

    uint64 FromRow = OriginalSlotIndex / MaxColumns;
    uint64 FromCol = OriginalSlotIndex % MaxColumns;
    uint64 ToRow = HoveredIndex / MaxColumns;
    uint64 ToCol = HoveredIndex % MaxColumns;
    EDirection Direction = GetMoveDirection(FromRow, FromCol, ToRow, ToCol);
    UE_LOG(LogTemp, Log, TEXT("MoveItem: Moving from (%d,%d) to (%d,%d), Direction=%d"),
        FromRow, FromCol, ToRow, ToCol, (uint8)Direction);

    if (Items[HoveredIndex].ReferencedActorClass)
    {
        FItem TempItem = Items[HoveredIndex];
        Items[HoveredIndex] = DraggedItem;
        Items[OriginalSlotIndex] = TempItem;
        UpdateSlotUI(OriginalSlotIndex);
        UpdateSlotUI(HoveredIndex);
        UE_LOG(LogTemp, Log, TEXT("MoveItem: Swapped item %d with item in slot %d"), DraggedItem.Index, HoveredIndex);
    }
    else
    {
        Items[HoveredIndex] = DraggedItem;
        Items[OriginalSlotIndex] = FItem();
        UpdateSlotUI(OriginalSlotIndex);
        UpdateSlotUI(HoveredIndex);
        UE_LOG(LogTemp, Log, TEXT("MoveItem: Moved item %d to empty slot %d"), DraggedItem.Index, HoveredIndex);
    }

    DraggedItemIndex = HoveredIndex;
    OriginalSlotIndex = HoveredIndex;

    if (Grid) Grid->ForceLayoutPrepass();
    if (Canvas) Canvas->ForceLayoutPrepass();
}

EDirection UInventory::GetMoveDirection(uint32 RowA, uint32 ColA, uint32 RowB, uint32 ColB) const
{
    if (RowA == RowB && ColA < ColB) return EDirection::Right;
    if (RowA == RowB && ColA > ColB) return EDirection::Left;
    if (ColA == ColB && RowA < RowB) return EDirection::Down;
    if (ColA == ColB && RowA > RowB) return EDirection::Up;
    return EDirection::None;
}

void UInventory::ShiftItems(uint32 StartIndex, uint32 EndIndex, EDirection Direction, bool bUpdateUI)
{
    if (!Items.IsValidIndex(StartIndex) || !Items.IsValidIndex(EndIndex)) return;
    if (StartIndex == EndIndex) return;

    int32 Step = 0;
    if (Direction == EDirection::Left || Direction == EDirection::Right)
    {
        Step = (Direction == EDirection::Right) ? 1 : -1;
        if (EndIndex / MaxColumns != StartIndex / MaxColumns) return;
    }
    else if (Direction == EDirection::Up || Direction == EDirection::Down)
    {
        Step = (Direction == EDirection::Down) ? static_cast<int32>(MaxColumns) : -static_cast<int32>(MaxColumns);
        if (EndIndex % MaxColumns != StartIndex % MaxColumns) return;
    }
    else
    {
        return;
    }

    TArray<uint64> Indices;
    int32 CurrentIndex = StartIndex;
    while (true)
    {
        Indices.Add(CurrentIndex);
        if (CurrentIndex == EndIndex) break;
        CurrentIndex += Step;
    }

    if (Step > 0)
    {
        for (int32 i = Indices.Num() - 1; i > 0; --i)
        {
            Items[Indices[i]] = Items[Indices[i - 1]];
            if (bUpdateUI) UpdateSlotUI(Indices[i]);
        }
    }
    else
    {
        for (int32 i = 0; i < Indices.Num() - 1; ++i)
        {
            Items[Indices[i]] = Items[Indices[i + 1]];
            if (bUpdateUI) UpdateSlotUI(Indices[i]);
        }
    }
}

void UInventory::UpdateSlotUI(uint32 SlotIndex)
{
    if (!Items.IsValidIndex(SlotIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateSlotUI: Invalid SlotIndex=%d"), SlotIndex);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("UpdateSlotUI: Updating slot %d, HasItem=%s"),
        SlotIndex, Items[SlotIndex].ReferencedActorClass ? TEXT("True") : TEXT("False"));

    if (Items[SlotIndex].ReferencedActorClass)
    {
        CreateItemIcon(SlotIndex);
        CreateIconCounterText(SlotIndex);
    }
    else
    {
        RemoveItemIcon(SlotIndex);
    }

    if (ForegroundBorders.IsValidIndex(SlotIndex))
    {
        ForegroundBorders[SlotIndex]->SetVisibility(ESlateVisibility::Visible);
        ForegroundBorders[SlotIndex]->ForceLayoutPrepass();
        UE_LOG(LogTemp, Log, TEXT("UpdateSlotUI: Slot %d border updated"), SlotIndex);
    }
}

void UInventory::CreateItemIcon(uint32 SlotIndex)
{
    if (!Items.IsValidIndex(SlotIndex) || !ForegroundBorders.IsValidIndex(SlotIndex)) return;

    TObjectPtr<USizeBox> SizeBox = Cast<USizeBox>(ForegroundBorders[SlotIndex]->GetContent());
    if (!SizeBox) return;

    SizeBox->ClearChildren();

    UOverlay* IconOverlay = NewObject<UOverlay>(this);
    IconOverlay->SetVisibility(ESlateVisibility::Visible);
    SizeBox->SetContent(IconOverlay);

    UImage* ItemIcon = NewObject<UImage>(this);
    ItemIcon->SetVisibility(ESlateVisibility::Visible);
    UOverlaySlot* ImageSlot = IconOverlay->AddChildToOverlay(ItemIcon);
    ImageSlot->SetHorizontalAlignment(HAlign_Fill);
    ImageSlot->SetVerticalAlignment(VAlign_Fill);

    if (Items[SlotIndex].IconTexture.IsValid())
    {
        ItemIcon->SetBrushFromTexture(Items[SlotIndex].IconTexture.Get());
    }
    else
    {
        ItemIcon->SetColorAndOpacity(FLinearColor::Blue);
    }
}

void UInventory::CreateIconCounterText(uint32 SlotIndex)
{
    if (!ForegroundBorders.IsValidIndex(SlotIndex) || !Items.IsValidIndex(SlotIndex)) return;
    if (!Items[SlotIndex].ReferencedActorClass) return;

    TObjectPtr<USizeBox> SizeBox = Cast<USizeBox>(ForegroundBorders[SlotIndex]->GetContent());
    if (!SizeBox) return;

    UOverlay* IconOverlay = Cast<UOverlay>(SizeBox->GetContent());
    if (!IconOverlay)
    {
        IconOverlay = NewObject<UOverlay>(this);
        IconOverlay->SetVisibility(ESlateVisibility::Visible);
        SizeBox->SetContent(IconOverlay);
    }

    UTextBlock* ItemCounterText = nullptr;
    for (int32 i = 0; i < IconOverlay->GetChildrenCount(); ++i)
    {
        if (UTextBlock* Text = Cast<UTextBlock>(IconOverlay->GetChildAt(i)))
        {
            ItemCounterText = Text;
            break;
        }
    }

    if (!ItemCounterText)
    {
        ItemCounterText = NewObject<UTextBlock>(this);
        ItemCounterText->SetVisibility(ESlateVisibility::Visible);
        UOverlaySlot* TextOverlaySlot = IconOverlay->AddChildToOverlay(ItemCounterText);
        TextOverlaySlot->SetHorizontalAlignment(HAlign_Center);
        TextOverlaySlot->SetVerticalAlignment(VAlign_Center);
    }

    ItemCounterText->SetText(FText::AsNumber(Items[SlotIndex].Index));
    ItemCounterText->SetColorAndOpacity(FLinearColor::Red);
    ItemCounterText->SetJustification(ETextJustify::Center);
    ItemCounterText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 20));
}

uint32 UInventory::FindFirstEmptySlot() const
{
    for (uint64 i = 0; i < static_cast<uint64>(Items.Num()); i++)
    {
        if (!Items[i].ReferencedActorClass) return i;
    }
    return INDEX_NONE;
}

void UInventory::RemoveItemIcon(uint32 SlotIndex)
{
    if (ForegroundBorders.IsValidIndex(SlotIndex))
    {
        if (TObjectPtr<USizeBox> SizeBox = Cast<USizeBox>(ForegroundBorders[SlotIndex]->GetContent()))
        {
            SizeBox->ClearChildren();
            SizeBox->ForceLayoutPrepass();
        }
    }
}

void UInventory::Create()
{
    if (!Grid || !WidgetTree) return;

    Grid->ClearChildren();
    ForegroundBorders.Empty();
    IconSlots.Empty();
    Items.Empty();
    bCounterTextUpdated.Empty();

    Items.SetNum(MaxRows * MaxColumns);
    ForegroundBorders.SetNum(MaxRows * MaxColumns);
    IconSlots.SetNum(MaxRows * MaxColumns);
    bCounterTextUpdated.Init(false, MaxRows * MaxColumns);

    for (uint64 Rows = 0; Rows < MaxRows; Rows++)
    {
        for (uint64 Columns = 0; Columns < MaxColumns; Columns++)
        {
            uint64 Index = Rows * MaxColumns + Columns;

            UBorder* SlotBorder = NewObject<UBorder>(this);
            SlotBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f));
            SlotBorder->SetVisibility(ESlateVisibility::Visible);

            USizeBox* SizeBox = NewObject<USizeBox>(this);
            SizeBox->SetWidthOverride(100.0f);
            SizeBox->SetHeightOverride(100.0f);

            SlotBorder->SetContent(SizeBox);
            GridSlot = Grid->AddChildToUniformGrid(SlotBorder, Rows, Columns);
            GridSlot->SetHorizontalAlignment(HAlign_Center);
            GridSlot->SetVerticalAlignment(VAlign_Center);

            ForegroundBorders[Index] = SlotBorder;
            IconSlots[Index] = SizeBox;
            SlotBorder->ForceLayoutPrepass();
            SizeBox->ForceLayoutPrepass();
        }
    }

    if (Grid) Grid->ForceLayoutPrepass();
}

void UInventory::Open()
{
    SetVisibility(ESlateVisibility::Visible);
}

void UInventory::Close()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

bool UInventory::GetIsInventoryFull() const
{
    return bIsInventoryFull;
}

const TArray<FItem>& UInventory::GetItems() const
{
    return Items;
}

TArray<UBorder*> UInventory::GetForegroundBorders() const
{
    TArray<UBorder*> RawBorders;
    for (TObjectPtr<UBorder> Border : ForegroundBorders)
    {
        RawBorders.Add(Border.Get());
    }
    return RawBorders;
}

UUniformGridPanel* UInventory::GetGrid() const
{
    return Grid.Get();
}

EDirection UInventory::SortItem(FItem& MovedItem, FItem& ItemToMove)
{
    return EDirection::None;
}

uint32 UInventory::FindItemIndex(const FItem& TargetItem) const
{
    return INDEX_NONE;
}

void UInventory::ScheduleSlideAnimation(uint32 FromIndex, uint32 ToIndex, EDirection Direction)
{
    ScheduledFromIndex = FromIndex;
    ScheduledToIndex = ToIndex;
    ScheduledDirection = Direction;
    bAnimationScheduled = true;
}

void UInventory::StartSlideAnimation(uint32 FromIndex, uint32 ToIndex, EDirection Direction)
{
    // Placeholder for slide animation logic
}

FVector2D UInventory::GetSlotPosition(uint32 SlotIndex) const
{
    if (ForegroundBorders.IsValidIndex(SlotIndex) && ForegroundBorders[SlotIndex])
    {
        FGeometry SlotGeometry = ForegroundBorders[SlotIndex]->GetCachedGeometry();
        return SlotGeometry.LocalToAbsolute(FVector2D::ZeroVector);
    }
    return FVector2D::ZeroVector;
}

float UInventory::CustomEaseInOut(float T) const
{
    return T < 0.5f ? 2.0f * T * T : -1.0f + (4.0f - 2.0f * T) * T;
}

