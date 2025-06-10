#include "Inventory.h"

uint32 UInventory::ItemCounter = 0;

UInventory::UInventory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer),
      MaxRows(3),
      MaxColumns(4),
      bIsInventoryFull(false),
      Canvas(nullptr),
      BackgroundBorder(nullptr),
      BackgroundBorder_S(nullptr),
      Background_VB(nullptr),
      Title(nullptr),
      Title_VBS(nullptr),
      Grid(nullptr),
      Grid_VBS(nullptr),
      Grid_S(nullptr),
      DraggedItemWidget(nullptr),
      bPendingRemoval(false),
      DraggedItemIndex(INDEX_NONE),
      OriginalSlotIndex(INDEX_NONE),
      PreviousSlotIndex(INDEX_NONE),
      DraggedItem(FItem()),
      bIsItemDragging(false),
      bHasItemDragStarted(false),
      MousePosition(FVector2D::ZeroVector),
      bIsSliding(false),
      SlideFromIndex(INDEX_NONE),
      SlideToIndex(INDEX_NONE),
      SlideProgress(0.0f),
      SlideDuration(0.2f),
      SlidingItem(FItem()),
      ScheduledFromIndex(INDEX_NONE),
      ScheduledToIndex(INDEX_NONE),
      ScheduledDirection(EDirection::None)
{
    Items.SetNum(MaxRows * MaxColumns);
    ForegroundBorders.SetNum(MaxRows * MaxColumns);

    // Resetting Item Index Back To Zero When the Player Starts To Play
    ItemCounter = 0;
}

void UInventory::NativeOnInitialized()
{
    // Initialize widget memnber and layout before inventory construction 

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
    Background_VB = NewObject<UVerticalBox>(this);
    BackgroundBorder = NewObject<UBorder>(this);
    BackgroundBorder->SetBrushColor(FLinearColor::Gray);
    BackgroundBorder->SetPadding(FMargin(7.5f, 0.0f, 7.5f, 0.0f));
  
    // Name the inventory "Inventory"
    Title = NewObject<UTextBlock>(this);
    Title->SetText(FText::FromString(TEXT("Inventory")));
    
    // Need a vertical bocx slot for title middle adjustment
    Title_VBS = Background_VB->AddChildToVerticalBox(Title);
    Title_VBS->SetHorizontalAlignment(HAlign_Center);
    Title_VBS->SetVerticalAlignment(VAlign_Top);
    Title_VBS->SetPadding(FMargin(10, 10, 10, 10));

    // Create a grid for the inevntory and readjust grid alignment within baackground's vertical box 
    Grid = NewObject<UUniformGridPanel>(this);
    Grid->SetSlotPadding(FMargin(7.0f, 7.0f, 7.0f, 7.0f));
    Grid_VBS = Background_VB->AddChildToVerticalBox(Grid);
    Grid_VBS->SetHorizontalAlignment(HAlign_Fill);
    Grid_VBS->SetVerticalAlignment(VAlign_Fill);

    // Setting BackgourndBorder content to what is inside the BackgroundBorder's VerticalBox and position/anchoring
    BackgroundBorder->SetContent(Background_VB);
    BackgroundBorder_S = Canvas->AddChildToCanvas(BackgroundBorder);
    BackgroundBorder_S->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
    BackgroundBorder_S->SetAlignment(FVector2D(1.0f, 0.0f));
    BackgroundBorder_S->SetOffsets(FMargin(-10.0f, 11.0f,485.0f, 419.0f));

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
        uint32 HoveredIndex = FindHoveredItemIndex(InMouseEvent);
        if (HoveredIndex != INDEX_NONE && Items.IsValidIndex(HoveredIndex))
        {
            DraggedItemIndex = HoveredIndex;
            OriginalSlotIndex = HoveredIndex;

            DraggedItem = Items[HoveredIndex];

            bIsItemDragging = true;
            bHasItemDragStarted = false;

            MousePosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

            return FReply::Handled();
        }
    }
    return FReply::Unhandled();
}

// Continue from here
FReply UInventory::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsItemDragging == false)
        return FReply::Unhandled();

    Super::NativeOnMouseMove(InGeometry, InMouseEvent);

    // Get mouse position in relation to the widget that mouse is hovering
    MousePosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

    // Creating copy of item outside the inventory and making it follow the mouse icon
    if (bHasItemDragStarted && (DraggedItemWidget != nullptr))
    {
        if (UCanvasPanelSlot* WidgetSlot = Cast<UCanvasPanelSlot>(DraggedItemWidget->Slot))
        {
            WidgetSlot->SetPosition(MousePosition - FVector2D(50.0f, 50.0f));
        }
        return FReply::Handled();
    }

    // Otherwise, check whether cursor just crossed outside the original slot:

    // Force layout so slot geometry is up to date
    RefreshInventoryUI();

    bool bIsOutside = false;

    // Get Absolute mouse position in relation to the top lef of the screen
    MousePosition = InMouseEvent.GetScreenSpacePosition();

    if (ForegroundBorders.IsValidIndex(DraggedItemIndex) && ForegroundBorders[DraggedItemIndex])
    {
        const FGeometry SlotGeom = ForegroundBorders[DraggedItemIndex]->GetCachedGeometry();
        const FVector2D SlotTopLeft = SlotGeom.LocalToAbsolute(FVector2D::ZeroVector);
        const FVector2D SlotSize = SlotGeom.GetLocalSize();
        const FVector2D SlotBotRight = SlotTopLeft + SlotSize;

        if (SlotSize.X < 10.0f || SlotSize.Y < 10.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT(
                "NativeOnMouseMove: Slot %d geometry invalid: Size=%s. Skipping edge check."),
                DraggedItemIndex, *SlotSize.ToString());
            return FReply::Handled();
        }

        const int32 Row = DraggedItemIndex / MaxColumns;
        const int32 Col = DraggedItemIndex % MaxColumns;
        const float SlotPadding = 1.0f;
        const float Buffer = 1.0f;

        // If the cursor moved beyond any edge of the original slot,
        // we treat it as “popped out.”
        if (Row == 0 && MousePosition.Y < SlotTopLeft.Y - SlotPadding - Buffer)
        {
            bIsOutside = true;
        }
        else if (Row == MaxRows - 1 && MousePosition.Y > SlotBotRight.Y + SlotPadding + Buffer)
        {
            bIsOutside = true;
        }
        else if (Col == 0 && MousePosition.X < SlotTopLeft.X - SlotPadding - Buffer)
        {
            bIsOutside = true;
        }
        else if (Col == MaxColumns - 1 && MousePosition.X > SlotBotRight.X + SlotPadding + Buffer)
        {
            bIsOutside = true;
        }

        if (bIsOutside)
        {
            // ───────────────────────────────────────────────────────────────
            // 1) “Unparent” the slot’s icon by clearing only its USizeBox children,
            //    making the slot appear empty without destroying the UBorder/SizeBox.
            // ───────────────────────────────────────────────────────────────
            UBorder* SlotBorder = ForegroundBorders[DraggedItemIndex];
            if (SlotBorder)
            {
                if (USizeBox* SizeBox = Cast<USizeBox>(SlotBorder->GetContent()))
                {
                    SizeBox->ClearChildren();
                }
            }

            // 2) Mark that drag‐out has begun
            bHasItemDragStarted = true;

            // 3) Build the floating widget at the mouse position
            if (Canvas)
            {
                DraggedItemWidget = NewObject<UOverlay>(this);
                DraggedItemWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

                // (a) Icon or blue square
                UImage* ItemImage = NewObject<UImage>(this);
                ItemImage->SetVisibility(ESlateVisibility::Visible);
                if (DraggedItem.Texture.IsValid())
                {
                    ItemImage->SetBrushFromTexture(DraggedItem.Texture.Get());
                }
                else
                {
                    ItemImage->SetColorAndOpacity(FLinearColor::Blue);
                }
                UOverlaySlot* ImgSlot = DraggedItemWidget->AddChildToOverlay(ItemImage);
                ImgSlot->SetHorizontalAlignment(HAlign_Fill);
                ImgSlot->SetVerticalAlignment(VAlign_Fill);

                // (b) Red index text
                UTextBlock* CounterText = NewObject<UTextBlock>(this);
                CounterText->SetVisibility(ESlateVisibility::Visible);
                CounterText->SetText(FText::AsNumber(DraggedItem.Index));
                CounterText->SetColorAndOpacity(FLinearColor::Red);
                CounterText->SetJustification(ETextJustify::Center);
                CounterText->SetFont(
                    FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 20));
                UOverlaySlot* TxtSlot = DraggedItemWidget->AddChildToOverlay(CounterText);
                TxtSlot->SetHorizontalAlignment(HAlign_Center);
                TxtSlot->SetVerticalAlignment(VAlign_Center);

                if (UCanvasPanelSlot* WidgetSlot = Canvas->AddChildToCanvas(DraggedItemWidget))
                {
                    WidgetSlot->SetSize(FVector2D(100.0f, 100.0f));
                    WidgetSlot->SetPosition(MousePosition - FVector2D(50.0f, 50.0f));
                    WidgetSlot->SetZOrder(100);
                }
            }

            UE_LOG(LogTemp, Log, TEXT(
                "Drag started for item %d from slot %d"),
                DraggedItem.Index, DraggedItemIndex);
        }
    }

    // Always call MoveItem even after pop‐out (to handle interior sorting)
    MoveItem(InMouseEvent);
    return FReply::Handled();
}

FReply UInventory::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!bIsItemDragging)
    {
        return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
    }

    // 1) Force a layout pass so that hit tests are accurate
    if (Canvas) Canvas->ForceLayoutPrepass();
    if (Grid)   Grid->ForceLayoutPrepass();
    for (auto& CurrentBorder : ForegroundBorders)
    {
        if (CurrentBorder) CurrentBorder->ForceLayoutPrepass();
    }

    // 2) Figure out which slot (if any) the mouse is over now
    const uint32 HoveredIndex = FindHoveredItemIndex(InMouseEvent);
    UE_LOG(LogTemp, Warning, TEXT("OnMouseButtonUp: HoveredIndex = %u"), HoveredIndex);

    // 3) Remove the floating widget if it exists
    if (DraggedItemWidget && Canvas)
    {
        Canvas->RemoveChild(DraggedItemWidget);
        DraggedItemWidget = nullptr;
    }

    // 4) Determine whether OriginalSlotIndex was on an edge of the grid
    const uint32 OrigRow = OriginalSlotIndex / MaxColumns;
    const uint32 OrigCol = OriginalSlotIndex % MaxColumns;
    const bool bOriginalWasEdge =
        (OrigRow == 0) ||
        (OrigRow == MaxRows - 1) ||
        (OrigCol == 0) ||
        (OrigCol == MaxColumns - 1);

    // 5) Check if mouse is still inside the grid’s bounding box
    bool bMouseInsideGrid = false;
    if (Grid)
    {
        const FGeometry GridGeom = Grid->GetCachedGeometry();
        const FVector2D TopLeft = GridGeom.GetAbsolutePosition();
        const FVector2D BottomRight = TopLeft + GridGeom.GetAbsoluteSize();
        const FVector2D MouseScreenPos = InMouseEvent.GetScreenSpacePosition();

        bMouseInsideGrid =
            MouseScreenPos.X >= TopLeft.X && MouseScreenPos.X <= BottomRight.X &&
            MouseScreenPos.Y >= TopLeft.Y && MouseScreenPos.Y <= BottomRight.Y;
    }

    bool bHandled = false;

    // ─────────────────────────────────────────────────────────────────────
    // New interior‐move / swap / restore logic
    // ─────────────────────────────────────────────────────────────────────
    if (bMouseInsideGrid && HoveredIndex != INDEX_NONE && Items.IsValidIndex(HoveredIndex))
    {
        // === CASE A: the target slot is EMPTY (no WorldObjectReverence) ===
        if (!Items[HoveredIndex].WorldObjectReverence)
        {
            // 1) Put the dragged item into the empty slot
            Items[HoveredIndex] = DraggedItem;
            UpdateSlotUI(HoveredIndex);

            // 2) If the item was still in the inventory (not already popped out), remove it now
            if (!bPendingRemoval && OriginalSlotIndex != INDEX_NONE)
            {
                RemoveItem(OriginalSlotIndex);
                UpdateSlotUI(OriginalSlotIndex);
            }
            // If bPendingRemoval == true, then the slot was already cleared when we popped out,
            // so there’s nothing left to remove.

            bHandled = true;
        }
        // === CASE B: dropped back onto the same slot we started from ===
        else if (HoveredIndex == OriginalSlotIndex)
        {
            // 1) If we never removed from Items[] (bPendingRemoval == false), just restore in place.
            if (!bPendingRemoval && OriginalSlotIndex != INDEX_NONE)
            {
                Items[OriginalSlotIndex] = DraggedItem;
                UpdateSlotUI(OriginalSlotIndex);
            }
            // 2) If bPendingRemoval == true, that means the slot was cleared when we popped out.
            //    To restore, we must re‐insert the dragged item back into OriginalSlotIndex now.
            else if (bPendingRemoval && OriginalSlotIndex != INDEX_NONE)
            {
                Items[OriginalSlotIndex] = DraggedItem;
                UpdateSlotUI(OriginalSlotIndex);
            }

            bHandled = true;
        }
        // === CASE C: target slot is occupied by a different item → SWAP ===
        else
        {
            // Save the item that was in the hovered slot
            FItem Temp = Items[HoveredIndex];

            // Put the dragged item into HoveredIndex
            Items[HoveredIndex] = DraggedItem;
            UpdateSlotUI(HoveredIndex);

            // Now decide where Temp goes
            if (!bOriginalWasEdge && !bPendingRemoval)
            {
                // Interior drag & item was still in inventory: swap‐out goes back to the original slot
                Items[OriginalSlotIndex] = Temp;
                UpdateSlotUI(OriginalSlotIndex);
            }
            else
            {
                // Edge‐drag OR item was already popped out (bPendingRemoval==true):
                // Put Temp into the first empty slot, or back to OriginalSlotIndex if none found.
                const uint32 FirstEmpty = FindFirstEmptySlot();
                if (FirstEmpty != INDEX_NONE)
                {
                    Items[FirstEmpty] = Temp;
                    UpdateSlotUI(FirstEmpty);
                }
                else if (OriginalSlotIndex != INDEX_NONE)
                {
                    // Only restore to OriginalSlotIndex if it wasn’t already re‐used
                    Items[OriginalSlotIndex] = Temp;
                    UpdateSlotUI(OriginalSlotIndex);
                }
            }

            // If the dragged item was still in the inventory (bPendingRemoval == false),
            // we now remove it from the old slot. If bPendingRemoval==true, that slot is already empty.
            if (!bPendingRemoval && OriginalSlotIndex != INDEX_NONE)
            {
                RemoveItem(OriginalSlotIndex);
                UpdateSlotUI(OriginalSlotIndex);
            }

            bHandled = true;
        }
    }
    // ─────────────────────────────────────────────────────────────────
    // 2) Dropped outside the grid but this was an interior drag → RESTORE
    // ─────────────────────────────────────────────────────────────────
    else if (!bOriginalWasEdge && !bPendingRemoval)
    {
        // Put the dragged item back into its original slot
        if (OriginalSlotIndex != INDEX_NONE)
        {
            Items[OriginalSlotIndex] = DraggedItem;
            UpdateSlotUI(OriginalSlotIndex);
        }
        bHandled = true;
    }

    // ─────────────────────────────────────────────────────────────────
    // CASE D “pop‐out” spawn logic (edge‐drag branch)
    // Only runs if we haven’t already handled an interior drop
    // and if the drag started on an edge OR is pending removal.
    // ─────────────────────────────────────────────────────────────────
    if (!bHandled)
    {
        UE_LOG(LogTemp, Log, TEXT("--- Entering pop‐out / edge‐drag branch ---"));

        if (DraggedItem.StaticMesh.IsValid())
        {
            UE_LOG(LogTemp, Log, TEXT("Spawning actor with mesh: %s"), *DraggedItem.StaticMesh.ToString());
        }
        else if (DraggedItem.WorldObjectReverence)
        {
            UE_LOG(LogTemp, Log, TEXT("Spawning WorldObjectReverence: %s"), *DraggedItem.WorldObjectReverence->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("DraggedItem has neither StaticMesh nor WorldObjectReverence"));
        }

        UWorld* World = GetWorld();
        if (World)
        {
            // Use the stored WorldObjectTransform (force scale = 1)
            const FTransform& ItemTransform = DraggedItem.WorldObjectTransform;
            const FVector      ItemLocation = ItemTransform.GetLocation();
            const FRotator     ItemRotation = ItemTransform.GetRotation().Rotator();

            UE_LOG(LogTemp, Log,
                TEXT("DraggedItem.WorldObjectTransform → Location=(%.3f, %.3f, %.3f), Rotation=(%.3f, %.3f, %.3f)"),
                ItemLocation.X, ItemLocation.Y, ItemLocation.Z,
                ItemRotation.Pitch, ItemRotation.Yaw, ItemRotation.Roll);

            // Draw a debug sphere so you see where it spawns
            DrawDebugSphere(World, ItemLocation, 25.0f, 12, FColor::Blue, false, 5.0f);

            // Set up a spawn transform with unit scale
            FTransform SpawnTransform;
            SpawnTransform.SetLocation(ItemLocation);
            SpawnTransform.SetRotation(ItemTransform.GetRotation());
            SpawnTransform.SetScale3D(ItemTransform.GetScale3D());

            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride =
                ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

            // Spawn the AStaticMeshActor in the world
            AStaticMeshActor* MeshActor =
                World->SpawnActor<AStaticMeshActor>(
                    AStaticMeshActor::StaticClass(),
                    SpawnTransform,
                    SpawnParams
                );
            MeshActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);

            // Assign the static mesh if it exists
            if (DraggedItem.StaticMesh.IsValid())
            {
                UStaticMesh* LoadedMesh = DraggedItem.StaticMesh.LoadSynchronous();
                if (LoadedMesh)
                {
                    MeshActor->GetStaticMeshComponent()->SetStaticMesh(LoadedMesh);
                    UE_LOG(LogTemp, Log, TEXT("Assigned StaticMesh: %s"), *LoadedMesh->GetName());
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Failed to load StaticMesh."));
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("DraggedItem.StaticMesh is not valid."));
            }

            // Apply stored materials if any
            if (MeshActor->GetStaticMeshComponent())
            {
                for (int32 Index = 0; Index < DraggedItem.StoredMaterials.Num(); ++Index)
                {
                    if (DraggedItem.StoredMaterials[Index].IsValid())
                    {
                        UMaterialInterface* Mat = DraggedItem.StoredMaterials[Index].LoadSynchronous();
                        if (Mat)
                        {
                            MeshActor->GetStaticMeshComponent()->SetMaterial(Index, Mat);
                            UE_LOG(LogTemp, Log,
                                TEXT("Applied material to slot %d: %s"),
                                Index, *Mat->GetName()
                            );
                        }
                        else
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Failed to load material at slot %d."), Index);
                        }
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Invalid material pointer at slot %d."), Index);
                    }
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("No materials found for 'StaticMeshComponent'."));
            }

            // Remove from inventory *once* (if not already removed)
            if (!bPendingRemoval && OriginalSlotIndex != INDEX_NONE)
            {
                RemoveItem(OriginalSlotIndex);
                UpdateSlotUI(OriginalSlotIndex);
            }

            // Mark that we've now popped out
            bPendingRemoval = true;
            bHandled = true;
        }
    }

    // 6) Clear drag state & force a final redraw of all slots
    bIsItemDragging = false;
    bHasItemDragStarted = false;
    bPendingRemoval = false;   // reset for next drag
    DraggedItem = FItem();
    DraggedItemIndex = INDEX_NONE;
    PreviousSlotIndex = INDEX_NONE;
    OriginalSlotIndex = INDEX_NONE;

    if (bHandled)
    {
        for (uint32 i = 0; i < static_cast<uint32>(Items.Num()); ++i)
        {
            UpdateSlotUI(i);
        }
    }

    return FReply::Handled().ReleaseMouseCapture();
}

void UInventory::AddItem(AActor* ItemActor)
{
    if (!ItemActor) return;

    // If the inventory is already full, do nothing.
    if (ItemCounter >= static_cast<uint64>(MaxRows * MaxColumns))
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
    NewItem.WorldObjectReverence = ItemActor->GetClass();

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
            UE_LOG(LogTemp, Warning, TEXT("Sattic Mesh Name: %s"),  *NewItem.StaticMesh->GetName());
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
    UpdateSlotUI(EmptySlotIndex);

    // Destroy the actor we just picked up.
    ItemActor->Destroy();

    // Increment counter and check if the inventory is now full.
    ItemCounter++;
    bIsInventoryFull = (FindFirstEmptySlot() == INDEX_NONE);
}

void UInventory::RemoveItem(int32 SlotIndex)
{
    if (!Items.IsValidIndex(SlotIndex) || !Items[SlotIndex].WorldObjectReverence)
    {
        UE_LOG(LogTemp, Warning, TEXT("RemoveItem: Invalid slot %d or no item to remove"), SlotIndex);
        return;
    }

    Items[SlotIndex] = FItem();
    UpdateSlotUI(SlotIndex);   // <-- this ensures the slot is cleared visually

    if (ItemCounter > 0) ItemCounter--;

    uint64 NewIndex = 0;
    for (uint64 i = 0; i < static_cast<uint64>(Items.Num()); ++i)
    {
        if (Items[i].WorldObjectReverence)
        {
            Items[i].Index = NewIndex++;
            CreateIconCounterText(i);
        }
    }
    ItemCounter = NewIndex;
    bIsInventoryFull = (FindFirstEmptySlot() == INDEX_NONE);
}

uint32 UInventory::FindHoveredItemIndex(const FPointerEvent& InMouseEvent)
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
    float   MinDistance  = FLT_MAX;
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
            const FGeometry SlotGeometry       = ForegroundBorders[Index]->GetCachedGeometry();
            const FVector2D SlotAbsTopLeft      = SlotGeometry.LocalToAbsolute(FVector2D::ZeroVector);
            const FVector2D SlotAbsSize         = SlotGeometry.GetLocalSize();
            const FVector2D SlotAbsBottomRight  = SlotAbsTopLeft + SlotAbsSize;

            bAnyValidGeometry = true;

            // Check if MousePos is inside this slot's rectangle
            if (MousePos.X >= SlotAbsTopLeft.X && MousePos.X <= SlotAbsBottomRight.X &&
                MousePos.Y >= SlotAbsTopLeft.Y && MousePos.Y <= SlotAbsBottomRight.Y)
            {
                // Compute center-based distance so that if two slots overlap (rare),
                // we pick the closer center.
                const FVector2D SlotCenter = SlotAbsTopLeft + (SlotAbsSize * 0.5f);
                const float     Distance   = FVector2D::Distance(MousePos, SlotCenter);

                if (Distance < MinDistance)
                {
                    MinDistance    = Distance;
                    ClosestIndex   = Index;
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
    if (BackgroundBorder) BackgroundBorder->ForceLayoutPrepass();
    if (Grid) Grid->ForceLayoutPrepass();
    if (Canvas) Canvas->ForceLayoutPrepass();

    for (TObjectPtr<UBorder> Border : ForegroundBorders)
        if (Border) Border->ForceLayoutPrepass();
}

void UInventory::MoveItem(const FPointerEvent& MouseEvent)
{
    if (!bIsItemDragging)
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

    if (Items[HoveredIndex].WorldObjectReverence)
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

void UInventory::UpdateSlotUI(uint32 SlotIndex)
{
    if (!Items.IsValidIndex(SlotIndex) || !ForegroundBorders.IsValidIndex(SlotIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateSlotUI: Invalid SlotIndex=%d"), SlotIndex);
        return;
    }

    UBorder* SlotBorder = ForegroundBorders[SlotIndex];
    if (!SlotBorder) return;

    // We assume each UBorder’s content is always a USizeBox that we clear and repopulate.
    USizeBox* SizeBox = Cast<USizeBox>(SlotBorder->GetContent());
    if (!SizeBox) return;

    // 1) If dragging from an edge and this is the original slot, clear its icon but leave the border intact.
    if (bIsItemDragging && bHasItemDragStarted && SlotIndex == OriginalSlotIndex)
    {
        SizeBox->ClearChildren();
        return;
    }

    // 2) Otherwise, clear everything and redraw the actual item icon (with its red index) if present.
    SizeBox->ClearChildren();

    if (Items[SlotIndex].WorldObjectReverence)
    {
        CreateItemIcon(SlotIndex);
        CreateIconCounterText(SlotIndex);
    }
    // else: leave blank

    SlotBorder->SetVisibility(ESlateVisibility::Visible);
    SlotBorder->ForceLayoutPrepass();

    UE_LOG(LogTemp, Log, TEXT(
        "UpdateSlotUI: Slot %d border updated, HasItem=%s"),
        SlotIndex,
        Items[SlotIndex].WorldObjectReverence ? TEXT("True") : TEXT("False"));
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

    if (Items[SlotIndex].Texture.IsValid())
    {
        ItemIcon->SetBrushFromTexture(Items[SlotIndex].Texture.Get());
    }
    else
    {
        ItemIcon->SetColorAndOpacity(FLinearColor::Blue);
    }
}

void UInventory::CreateIconCounterText(uint32 SlotIndex)
{
    if (!ForegroundBorders.IsValidIndex(SlotIndex) || !Items.IsValidIndex(SlotIndex)) return;
    if (!Items[SlotIndex].WorldObjectReverence) return;

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
        if (!Items[i].WorldObjectReverence) return i;
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
            Grid_S = Grid->AddChildToUniformGrid(SlotBorder, Rows, Columns);
            Grid_S->SetHorizontalAlignment(HAlign_Center);
            Grid_S->SetVerticalAlignment(VAlign_Center);

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

FVector2D UInventory::GetSlotPosition(uint32 SlotIndex) const
{
    if (ForegroundBorders.IsValidIndex(SlotIndex) && ForegroundBorders[SlotIndex])
    {
        FGeometry SlotGeometry = ForegroundBorders[SlotIndex]->GetCachedGeometry();
        return SlotGeometry.LocalToAbsolute(FVector2D::ZeroVector);
    }
    return FVector2D::ZeroVector;
}
