#include "Inventory.h"

uint64 UInventory::ItemCounter = 0;

UInventory::UInventory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer),
      MaxRows(3),
      MaxColumns(4),
      bIsInventoryFull(false),
      Canvas(nullptr),
      BackgroundBorder(nullptr),
      BackgroundBorderSlot(nullptr),
      BackgroundVerticalBox(nullptr),
      Title(nullptr),
      TitleVerticalBoxSlot(nullptr),
      Grid(nullptr),
      GridVerticalBoxSlot(nullptr),
      GridSlot(nullptr),
      DraggedItemWidget(nullptr),
      HoveredSlot(INDEX_NONE),
      OriginalSlot(INDEX_NONE),
      DraggedItem(FItem()),
      MouseAbsolutePosition(FVector2D::ZeroVector),
      MouseLocalPosition(FVector2D::ZeroVector),
      DragState(EDragState::Null)
{
    // Set menber array's size to 12 (3x4)
    Items.SetNum(MaxRows * MaxColumns);
    ForegroundBorders.SetNum(MaxRows * MaxColumns);

    // Resetting item index back to zero when the player starts to play
    ItemCounter = 0;
}

void UInventory::NativeOnInitialized()
{
    // Initialize widget memnbers and layout before inventory construction 

    Super::NativeOnInitialized();

    if (!WidgetTree)
    {
        #if WITH_EDITOR
        UE_LOG(LogTemp, Error, TEXT("WidgetTree is null"));
        #else
        UE_LOG(LogTemp, Fatal, TEXT("WidgetTree is null"));
        #endif

        return;
    }

    Canvas = NewObject<UCanvasPanel>(this);

    // It's mandatory to set the first widget element as the root widget of the widget's tree
    WidgetTree->RootWidget = Canvas;

    // Create vertical Box to hold background border and create a gray backround boder
    BackgroundVerticalBox = NewObject<UVerticalBox>(this);
    BackgroundBorder = NewObject<UBorder>(this);
    BackgroundBorder->SetBrushColor(FLinearColor::Gray);
    BackgroundBorder->SetPadding(FMargin(7.5f, 0.0f, 7.5f, 0.0f));

    // Name the inventory "Inventory"
    Title = NewObject<UTextBlock>(this);
    Title->SetText(FText::FromString(TEXT("Inventory")));

    // Need a vertical bocx slot for title middle adjustment
    TitleVerticalBoxSlot = BackgroundVerticalBox->AddChildToVerticalBox(Title);
    TitleVerticalBoxSlot->SetHorizontalAlignment(HAlign_Center);
    TitleVerticalBoxSlot->SetVerticalAlignment(VAlign_Top);
    TitleVerticalBoxSlot->SetPadding(FMargin(10, 10, 10, 10));

    // Create a grid for the inevntory and readjust grid alignment within baackground's vertical box 
    Grid = NewObject<UUniformGridPanel>(this);
    Grid->SetSlotPadding(FMargin(7.0f, 7.0f, 7.0f, 7.0f));
    GridVerticalBoxSlot = BackgroundVerticalBox->AddChildToVerticalBox(Grid);
    GridVerticalBoxSlot->SetHorizontalAlignment(HAlign_Fill);
    GridVerticalBoxSlot->SetVerticalAlignment(VAlign_Fill);

    // Setting BackgourndBorder content to what is inside the BackgroundBorder's VerticalBox and position/anchoring
    BackgroundBorder->SetContent(BackgroundVerticalBox);
    BackgroundBorderSlot = Canvas->AddChildToCanvas(BackgroundBorder);
    BackgroundBorderSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
    BackgroundBorderSlot->SetAlignment(FVector2D(1.0f, 0.0f));
    BackgroundBorderSlot->SetOffsets(FMargin(-10.0f, 11.0f, 485.0f, 419.0f));

    // Call create method to colonize the inventory with slots
    Create();
}

void UInventory::NativeConstruct()
{
    Super::NativeConstruct();

    // Refresh inventory before it gets added to viewport
    RefreshInventoryUI();
}

FReply UInventory::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        HoveredSlot = FindHoveredSlot(InMouseEvent);

        // Does the CurrentHoveredSlot have a valid index
        if (HoveredSlot != INDEX_NONE && Items.IsValidIndex(HoveredSlot))
        {
            // Original slot is usefull for item falllback
            OriginalSlot = HoveredSlot;

            DraggedItem = Items[HoveredSlot];

            // Enum object responsible for dragging state
            DragState = EDragState::Select;

            MouseAbsolutePosition = InMouseEvent.GetScreenSpacePosition();

            // Grab root slate widget associate with this inventory
            TSharedPtr<SWidget> RootSlate = GetCachedWidget();

            if (!RootSlate.IsValid())
            {
                // Gracefully recover: early return, log a warning, etc.
                #if	WITH_EDITOR
                UE_LOG(LogTemp, Error, TEXT("Couldn't get cached root slate widget!"));
                #else
                UE_LOG(LogTemp, Fatal, TEXT("Couldn't get cached root slate widget!"));
                #endif

                return FReply::Unhandled();
            }

            // Keep track of any mouse actions by passing a refernce of Inventory's root slate widget to the CaptureMouse() event
            return FReply::Handled().CaptureMouse(RootSlate.ToSharedRef());
        }
    }
    return FReply::Unhandled();
}

FReply UInventory::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (DragState != EDragState::Select && DragState != EDragState::Moved)
        return Super::NativeOnMouseMove(InGeometry, InMouseEvent);

    Super::NativeOnMouseMove(InGeometry, InMouseEvent);

    uint64 Row;
    uint64 Column;

    MouseAbsolutePosition = InMouseEvent.GetScreenSpacePosition();
    MouseLocalPosition = InGeometry.AbsoluteToLocal(MouseAbsolutePosition);

    // Set dragged item to follow mouse cion with a centered alignment
    if (DragState == EDragState::Moved && DraggedItemWidget)
    {
        if (UCanvasPanelSlot* DraggedItemSlot = Cast<UCanvasPanelSlot>(DraggedItemWidget->Slot))
            DraggedItemSlot->SetPosition(MouseLocalPosition - FVector2D(50.0f, 50.0f));
        
        return FReply::Handled();
    }

    // Refresh inevntory so that there is no flickering on the inventory ui after moving item
    RefreshInventoryUI();


    if (HoveredSlot == INDEX_NONE || !ForegroundBorders.IsValidIndex(HoveredSlot))
        return FReply::Unhandled();

    if (UBorder* Border = ForegroundBorders[HoveredSlot].Get())
    {
        const FGeometry SlotGeometry = Border->GetCachedGeometry();

        const FVector2D SlotTopLeft = SlotGeometry.LocalToAbsolute(FVector2D::ZeroVector);
        const FVector2D SlotBottomRight = SlotTopLeft + SlotGeometry.GetLocalSize();

        Row = HoveredSlot / MaxColumns;
        Column = HoveredSlot % MaxColumns;

        bool bIsMouseOutOfSlotBounds = (Row == 0 && MouseAbsolutePosition.Y < SlotTopLeft.Y) ||
                                       (Row == MaxRows - 1 && MouseAbsolutePosition.Y > SlotBottomRight.Y) ||
                                       (Column == 0 && MouseAbsolutePosition.X < SlotTopLeft.X) ||
                                       (Column == MaxColumns - 1 && MouseAbsolutePosition.X > SlotBottomRight.X);

        if (bIsMouseOutOfSlotBounds)
        {
            // When item is selected and mouse outside slot bounds clear the original item before moving the overlay item
            if (USizeBox* Box = Cast<USizeBox>(Border->GetContent()))
            {
                Box->ClearChildren();
            }

            DragState = EDragState::Moved;

            // Create item overlay when moving item through slots
            DraggedItemWidget = CreateItemIcon(DraggedItem);

            if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(DraggedItemWidget))
            {
                CanvasSlot->SetSize(FVector2D(100.f, 100.f));
                CanvasSlot->SetPosition(MouseAbsolutePosition - FVector2D(50.f, 50.f));
                CanvasSlot->SetZOrder(100);
            }
        }
    }

    MoveItem(InMouseEvent, Row, Column);
    return FReply::Handled();
}

FReply UInventory::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Only finalize if we're in a drag at all (either Select or Moved)
    if (DragState != EDragState::Select && DragState != EDragState::Moved)
    {
        return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
    }

    // 1) Prepass for accurate hit tests
    RefreshInventoryUI();

    HoveredSlot = FindHoveredSlot(InMouseEvent);

    // 2) Tear down the ghost if it exists
    if (DraggedItemWidget && Canvas)
    {
        Canvas->RemoveChild(DraggedItemWidget);
        DraggedItemWidget = nullptr;
    }

    // 3) If dropped on a valid slot…        
    if (HoveredSlot != INDEX_NONE && Items.IsValidIndex(HoveredSlot))
    {
        // A) empty slot → move
        if (!Items[HoveredSlot].WorldObjectReference)
        {
            Items[HoveredSlot] = DraggedItem;
            RefreshInventoryUI();

            if (Items[OriginalSlot].WorldObjectReference)
            {
                RemoveItem(OriginalSlot);
                RefreshInventoryUI();
            }
        }
        // B) same slot → restore
        else if (HoveredSlot == OriginalSlot)
        {
            Items[OriginalSlot] = DraggedItem;
            RefreshInventoryUI();
        }
        // C) occupied → swap (with edge‑logic fallback)
        else
        {
            FItem HoveredItem = Items[HoveredSlot];
            Items[HoveredSlot] = DraggedItem;
            RefreshInventoryUI();

            bool bOrigEdge =
                (OriginalSlot / MaxColumns) == 0 ||
                (OriginalSlot / MaxColumns) == MaxRows - 1 ||
                (OriginalSlot % MaxColumns) == 0 ||
                (OriginalSlot % MaxColumns) == MaxColumns - 1;

            if (!bOrigEdge)
            {
                // interior swap
                Items[OriginalSlot] = HoveredItem;
                RefreshInventoryUI();
            }
            else
            {
                // edge: find first empty or fallback
                uint32 Empty = FindFirstEmptySlot();
                if (Empty != INDEX_NONE)
                {
                    Items[Empty] = HoveredItem;
                    RefreshInventoryUI();
                }
                else
                {
                    Items[OriginalSlot] = HoveredItem;
                    RefreshInventoryUI();
                }
            }

            // remove original if it still had the dragged item
            if (Items[OriginalSlot].WorldObjectReference)
            {
                RemoveItem(OriginalSlot);
                RefreshInventoryUI();
            }
        }
    }
    // 4) Otherwise, drop off‑grid → spawn actor in world
    else if (UWorld* World = GetWorld())
    {
        const FTransform& T = DraggedItem.WorldObjectTransform;
        DrawDebugSphere(World, T.GetLocation(), 25.f, 12, FColor::Blue, false, 5.f);

        FTransform S = T;
        S.SetScale3D(T.GetScale3D());
        FActorSpawnParameters P;
        P.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        if (auto* Actor = World->SpawnActor<AStaticMeshActor>(
            AStaticMeshActor::StaticClass(), S, P))
        {
            Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            if (DraggedItem.StaticMesh.IsValid())
                if (auto* Mesh = DraggedItem.StaticMesh.LoadSynchronous())
                    Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);

            for (int32 i = 0; i < DraggedItem.StoredMaterials.Num(); ++i)
                if (DraggedItem.StoredMaterials[i].IsValid())
                    if (auto* Mat = DraggedItem.StoredMaterials[i].LoadSynchronous())
                        Actor->GetStaticMeshComponent()->SetMaterial(i, Mat);
        }

        if (Items[OriginalSlot].WorldObjectReference)
        {
            RemoveItem(OriginalSlot);
        }
    }

    // 5) Finalize
    DragState = EDragState::Released;

    // 6) Refresh visuals
    RefreshInventoryUI();

    // Release the mouse capture so other widgets can receive events again
    return FReply::Handled().ReleaseMouseCapture();
}

void UInventory::AddItem(AActor* ItemActor)
{
    if (!ItemActor) return;

    // If the inventory is already full, do nothing.
    if (ItemCounter >= (MaxRows * MaxColumns))
    {
        bIsInventoryFull = true;
        return;
    }

    // Find the first empty slot.
    uint64 EmptySlotIndex = FindFirstEmptySlot();
    if (EmptySlotIndex == INDEX_NONE)
    {
        bIsInventoryFull = true;
        return;
    }

    // Populate a new FItem in that slot.
    FItem& NewItem = Items[EmptySlotIndex];
    NewItem = FItem();  // Reset to defaults.

    // Store the actor class so we can spawn it later if needed.
    NewItem.WorldObjectReference = ItemActor->GetClass();

    // Store the world transform at the moment of pickup.
    NewItem.WorldObjectTransform = ItemActor->GetActorTransform();

    // Store the index in the inventory.
    NewItem.Index = ItemCounter;

    // Attempt to grab the static mesh from the actor's components:
    if (UStaticMeshComponent* MeshComp = ItemActor->FindComponentByClass<UStaticMeshComponent>())
    {
        UE_LOG(LogTemp, Warning, TEXT("Mesh Component Name: %s"), *MeshComp->GetFName().ToString());
        if (MeshComp->GetStaticMesh())
        {
            // Save that mesh into our FItem so we can reassign it when spawning back into the world.
            NewItem.StaticMesh = MeshComp->GetStaticMesh();
            UE_LOG(LogTemp, Warning, TEXT("Sattic Mesh Name: %s"), *NewItem.StaticMesh->GetName());
        }

        for (int32 i = 0; i < MeshComp->GetNumMaterials(); ++i)
        {
            if (MeshComp->GetMaterial(i))
            {
                NewItem.StoredMaterials.Add(MeshComp->GetMaterial(i));
            }
        }
    }

    // Update the UI for this new slot.
    RefreshInventoryUI();

    // Destroy the actor we just picked up.
    ItemActor->Destroy();

    // Increment counter and check if the inventory is now full.
    ItemCounter++;
    bIsInventoryFull = (FindFirstEmptySlot() == INDEX_NONE);
}

void UInventory::RemoveItem(int32 SlotIndex)
{
    // 1) Validate
    if (!Items.IsValidIndex(SlotIndex) || !Items[SlotIndex].WorldObjectReference)
    {
        UE_LOG(LogTemp, Warning, TEXT("RemoveItem: Invalid slot %d or no item to remove"), SlotIndex);
        return;
    }

    // 2) Clear out the data for that slot
    Items[SlotIndex] = FItem();

    // 4) Decrement your global counter
    if (ItemCounter > 0)
    {
        --ItemCounter;
    }

    // 5) Re‑index any items that follow, and update their visuals in one pass
    uint64 NextIndex = 0;
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        if (Items[i].WorldObjectReference)
        {
            Items[i].Index = NextIndex++;
            CreateItemIcon(Items[SlotIndex]);
        }
    }

    // 6) Update your counter and fullness flag
    ItemCounter = NextIndex;
    bIsInventoryFull = (FindFirstEmptySlot() == INDEX_NONE);

    // 3) Refresh visuals in one go
    RefreshInventoryUI();
}

uint32 UInventory::FindHoveredSlot(const FPointerEvent& InMouseEvent)
{
    if (!Grid || ForegroundBorders.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("FindHoveredItemIndex: Grid or ForegroundBorders invalid"));
        return INDEX_NONE;
    }

    // Ensure parent/layout geometry is up to date
    RefreshInventoryUI();

    const FVector2D MousePos = InMouseEvent.GetScreenSpacePosition();
    uint64 ClosestIndex = INDEX_NONE;
    float   MinDistance = FLT_MAX;
    bool    bAnyValidGeometry = false;

    // Loop over every slot; use the one whose bounding box contains MousePos,
    // pick the closest center if multiple overlap.
    for (uint64 Row = 0; Row < MaxRows; ++Row)
    {
        for (uint64 Col = 0; Col < MaxColumns; ++Col)
        {
            const uint64 Index = Row * MaxColumns + Col;

            if (!ForegroundBorders.IsValidIndex(Index) || !ForegroundBorders[Index])
            {
                UE_LOG(LogTemp, Warning, TEXT(
                    "FindHoveredItemIndex: Slot %llu invalid or null"), Index);
                continue;
            }

            // Get the absolute geometry of this slot
            const FGeometry SlotGeometry = ForegroundBorders[Index]->GetCachedGeometry();
            const FVector2D SlotAbsTopLeft = SlotGeometry.LocalToAbsolute(FVector2D::ZeroVector);
            const FVector2D SlotAbsSize = SlotGeometry.GetLocalSize();
            const FVector2D SlotAbsBottomRight = SlotAbsTopLeft + SlotAbsSize;

            bAnyValidGeometry = true;

            // Check if MousePos is inside this slot's rectangle
            if (MousePos.X >= SlotAbsTopLeft.X && MousePos.X <= SlotAbsBottomRight.X &&
                MousePos.Y >= SlotAbsTopLeft.Y && MousePos.Y <= SlotAbsBottomRight.Y)
            {
                // Compute center-based distance so that if two slots overlap (rare),
                // we pick the closer center.
                const FVector2D SlotCenter = SlotAbsTopLeft + (SlotAbsSize * 0.5f);
                const float     Distance = FVector2D::Distance(MousePos, SlotCenter);

                if (Distance < MinDistance)
                {
                    MinDistance = Distance;
                    ClosestIndex = Index;
                    UE_LOG(LogTemp, Log, TEXT(
                        "FindHoveredItemIndex: Slot %llu hit, Distance=%f"), Index, Distance);
                }
            }
        }
    }

    if (ClosestIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning, TEXT(
            "FindHoveredItemIndex: No valid slot hit. AnyValidGeometry=%s, MousePos=%s"),
            bAnyValidGeometry ? TEXT("True") : TEXT("False"),
            *MousePos.ToString());
    }

    return static_cast<uint32>(ClosestIndex);
}

void UInventory::RefreshInventoryUI()
{
    // Ensure all parent layouts are up to date
    if (BackgroundBorder) BackgroundBorder->ForceLayoutPrepass();
    if (Grid)             Grid->ForceLayoutPrepass();
    if (Canvas)           Canvas->ForceLayoutPrepass();

    // Update every slot
    for (int32 Index = 0; Index < ForegroundBorders.Num(); ++Index)
    {
        // Validate slot index
        if (!Items.IsValidIndex(Index) || !ForegroundBorders.IsValidIndex(Index))
        {
            UE_LOG(LogTemp, Warning, TEXT("RefreshInventoryUI: Invalid Slot %d"), Index);
            continue;
        }

        UBorder* SlotBorder = ForegroundBorders[Index].Get();
        if (!SlotBorder)
        {
            continue;
        }

        USizeBox* SizeBox = Cast<USizeBox>(SlotBorder->GetContent());
        if (!SizeBox)
        {
            continue;
        }

        // Clear existing content
        SizeBox->ClearChildren();

        // If dragging and this is the original slot, leave it visually empty
        if (DragState == EDragState::Moved && Index == OriginalSlot)
        {
            continue;
        }

        // If item exists, recreate its icon
        if (Items[Index].WorldObjectReference)
        {
            UOverlay* IconOverlay = CreateItemIcon(Items[Index]);
            SizeBox->AddChild(IconOverlay);
        }

        SlotBorder->SetVisibility(ESlateVisibility::Visible);
        SlotBorder->ForceLayoutPrepass();

        UE_LOG(LogTemp, Log, TEXT("RefreshInventoryUI: Slot %d updated, HasItem=%s"),
            Index,
            Items[Index].WorldObjectReference ? TEXT("True") : TEXT("False"));
    }
}

void UInventory::MoveItem(const FPointerEvent& MouseEvent, uint64& HoveredSlotRow, uint64& HoveredSlotColumn)
{
    HoveredSlot = FindHoveredSlot(MouseEvent);

    if (HoveredSlot == INDEX_NONE || !Items.IsValidIndex(HoveredSlot))
    {
        #if	WITH_EDITOR
        UE_LOG(LogTemp, Error, TEXT("Hovered slot is not a valid inventory slot"));
        #else
        UE_LOG(LogTemp, Fatal, TEXT("Hovered slot is not a valid inventory slot"));
        #endif

        return;
    }

    uint64 OriginalSlotRow = HoveredSlot / MaxColumns;
    uint64 OriginalSlotColumn = HoveredSlot % MaxColumns;

    EDirection Direction = GetMoveDirection(OriginalSlotRow, OriginalSlotColumn, HoveredSlotRow, HoveredSlotColumn);

    if (Items[HoveredSlot].WorldObjectReference)
    {
        Items[OriginalSlot] = Items[HoveredSlot];
        Items[HoveredSlot] = DraggedItem;
    }
    else
    {
        Items[HoveredSlot] = DraggedItem;
        Items[OriginalSlot] = FItem();
    }

    OriginalSlot = HoveredSlot;

    RefreshInventoryUI();
}

EDirection UInventory::GetMoveDirection(uint32 RowA, uint32 ColA, uint32 RowB, uint32 ColB) const
{
    if (RowA == RowB && ColA < ColB) return EDirection::Right;
    if (RowA == RowB && ColA > ColB) return EDirection::Left;
    if (ColA == ColB && RowA < RowB) return EDirection::Down;
    if (ColA == ColB && RowA > RowB) return EDirection::Up;
    return EDirection::Null;
}

TObjectPtr<UOverlay> UInventory::CreateItemIcon(const FItem& item)
{
    UOverlay* IconOverlay = NewObject<UOverlay>(this);
    IconOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);

    // 1) Image
    UImage* ItemIcon = NewObject<UImage>(this);
    ItemIcon->SetVisibility(ESlateVisibility::Visible);
    ItemIcon->SetColorAndOpacity(FLinearColor::Blue);

    if (UOverlaySlot* ImgSlot = IconOverlay->AddChildToOverlay(ItemIcon))
    {
        ImgSlot->SetHorizontalAlignment(HAlign_Fill);
        ImgSlot->SetVerticalAlignment(VAlign_Fill);
    }

    // 2) Counter (if any)
    if (item.WorldObjectReference)
    {
        UTextBlock* CounterText = NewObject<UTextBlock>(this);
        CounterText->SetVisibility(ESlateVisibility::Visible);
        CounterText->SetText(FText::AsNumber(item.Index));
        CounterText->SetColorAndOpacity(FLinearColor::Red);
        CounterText->SetJustification(ETextJustify::Center);
        CounterText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 20));

        if (UOverlaySlot* TextSlot = IconOverlay->AddChildToOverlay(CounterText))
        {
            TextSlot->SetHorizontalAlignment(HAlign_Center);
            TextSlot->SetVerticalAlignment(VAlign_Center);
        }
    }

    return IconOverlay;
}

uint32 UInventory::FindFirstEmptySlot() const
{
    for (uint64 i = 0; i < static_cast<uint64>(Items.Num()); i++)
    {
        if (!Items[i].WorldObjectReference) return i;
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
    Items.Empty();

    Items.SetNum(MaxRows * MaxColumns);
    ForegroundBorders.SetNum(MaxRows * MaxColumns);

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

TArray<TObjectPtr<UBorder>> UInventory::GetForegroundBorders() const
{
    return ForegroundBorders;
}

TObjectPtr<UUniformGridPanel> UInventory::GetGrid() const
{
    return Grid.Get();
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
