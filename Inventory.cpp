#include "Inventory.h"

uint32 UInventory::ItemCounter = 0;

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
    DragStartSlot(INDEX_NONE),
    OriginalSlot(INDEX_NONE),
    PreviousSlotIndex(INDEX_NONE),
    DraggedItem(FItem()),
    MousePosition(FVector2D::ZeroVector),
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
        uint32 CurrentHoveredSlot = FindHoveredSlot(InMouseEvent);

        if (CurrentHoveredSlot != INDEX_NONE && Items.IsValidIndex(CurrentHoveredSlot))
        {
            const FItem& Item = Items[CurrentHoveredSlot];

            // ✅ Check if the item is actually valid (e.g., has a reference to an object or class)
            if (!Item.WorldObjectReference)
            {
                // ❌ Slot is visually there but logically empty: no dragging
                return FReply::Unhandled();
            }

            DragStartSlot = CurrentHoveredSlot;
            OriginalSlot = CurrentHoveredSlot;
            DraggedItem = Item; // ✅ Now safe
            DragState = EDragState::Select;
            MousePosition = InMouseEvent.GetScreenSpacePosition();

            TSharedPtr<SWidget> RootSlate = GetCachedWidget();
            if (!RootSlate.IsValid())
            {
#if	WITH_EDITOR
                UE_LOG(LogTemp, Error, TEXT("Couldn't get cached root slate widget!"));
#else
                UE_LOG(LogTemp, Fatal, TEXT("Couldn't get cached root slate widget!"));
#endif
                return FReply::Unhandled();
            }

            return FReply::Handled().CaptureMouse(RootSlate.ToSharedRef());
        }
    }
    return FReply::Unhandled();
}

FReply UInventory::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Only care if we’re “about to pop” or “already popped”
    if (DragState != EDragState::Select && DragState != EDragState::Moved)
    {
        return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
    }

    Super::NativeOnMouseMove(InGeometry, InMouseEvent);

    const FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();
    const FVector2D LocalPos = InGeometry.AbsoluteToLocal(ScreenPos);

    // — If already popped out, just update the ghost’s position —
    if (DragState == EDragState::Moved && DraggedItemWidget)
    {
        if (auto* GhostSlot = Cast<UCanvasPanelSlot>(DraggedItemWidget->Slot))
        {
            GhostSlot->SetPosition(LocalPos - FVector2D(50.f, 50.f));
        }
        return FReply::Handled();
    }

    // — Still in “Select”: check boundary to pop out —
    RefreshInventoryUI();

    if (UBorder* Border = ForegroundBorders[DragStartSlot].Get())
    {
        const FGeometry SlotGeom = Border->GetCachedGeometry();
        const FVector2D TL = SlotGeom.LocalToAbsolute(FVector2D::ZeroVector);
        const FVector2D BR = TL + SlotGeom.GetLocalSize();
        const int32 Row = DragStartSlot / MaxColumns;
        const int32 Col = DragStartSlot % MaxColumns;
        constexpr float Pad = 1.f, Buf = 1.f;

        bool bCrossed =
            (Row == 0 && ScreenPos.Y < TL.Y - Pad - Buf) ||
            (Row == MaxRows - 1 && ScreenPos.Y > BR.Y + Pad + Buf) ||
            (Col == 0 && ScreenPos.X < TL.X - Pad - Buf) ||
            (Col == MaxColumns - 1 && ScreenPos.X > BR.X + Pad + Buf);

        if (bCrossed)
        {
            // 1) Clear the slot’s SizeBox to leave it visually empty
            if (auto* Box = Cast<USizeBox>(Border->GetContent()))
            {
                Box->ClearChildren();
            }

            // 2) Mark popped-out
            DragState = EDragState::Moved;

            // 3) Spawn the ghost widget at cursor
            if (Canvas)
            {
                DraggedItemWidget = NewObject<UOverlay>(this);
                DraggedItemWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

                // (a) Icon
                UImage* Img = NewObject<UImage>(this);
                Img->SetVisibility(ESlateVisibility::Visible);
                    Img->SetColorAndOpacity(FLinearColor::Blue);
                auto* ImgSlot = DraggedItemWidget->AddChildToOverlay(Img);
                ImgSlot->SetHorizontalAlignment(HAlign_Fill);
                ImgSlot->SetVerticalAlignment(VAlign_Fill);

                // (b) Counter
                UTextBlock* Txt = NewObject<UTextBlock>(this);
                Txt->SetVisibility(ESlateVisibility::Visible);
                Txt->SetText(FText::AsNumber(DraggedItem.Index));
                Txt->SetColorAndOpacity(FLinearColor::Red);
                Txt->SetJustification(ETextJustify::Center);
                Txt->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 20));
                auto* TxtSlot = DraggedItemWidget->AddChildToOverlay(Txt);
                TxtSlot->SetHorizontalAlignment(HAlign_Center);
                TxtSlot->SetVerticalAlignment(VAlign_Center);

                if (auto* CanvasSlot = Canvas->AddChildToCanvas(DraggedItemWidget))
                {
                    CanvasSlot->SetSize(FVector2D(100.f, 100.f));
                    CanvasSlot->SetPosition(ScreenPos - FVector2D(50.f, 50.f));
                    CanvasSlot->SetZOrder(100);
                }
            }
        }
    }

    // Always run your interior MoveItem logic
    MoveItem(InMouseEvent);
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
    if (Canvas) Canvas->ForceLayoutPrepass();
    if (Grid)   Grid->ForceLayoutPrepass();
    for (auto& Ptr : ForegroundBorders)
        if (auto* B = Ptr.Get()) B->ForceLayoutPrepass();

    const FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();
    const uint32    HoveredIndex = FindHoveredSlot(InMouseEvent);

    // 2) Tear down the ghost if it exists
    if (DraggedItemWidget && Canvas)
    {
        Canvas->RemoveChild(DraggedItemWidget);
        DraggedItemWidget = nullptr;
    }

    // 3) If dropped on a valid slot…        
    if (HoveredIndex != INDEX_NONE && Items.IsValidIndex(HoveredIndex))
    {
        // A) empty slot → move
        if (!Items[HoveredIndex].WorldObjectReference)
        {
            Items[HoveredIndex] = DraggedItem;
            UpdateSlotUI(HoveredIndex);

            if (Items[OriginalSlot].WorldObjectReference)
            {
                RemoveItem(OriginalSlot);
                UpdateSlotUI(OriginalSlot);
            }
        }
        // B) same slot → restore
        else if (HoveredIndex == OriginalSlot)
        {
            Items[OriginalSlot] = DraggedItem;
            UpdateSlotUI(OriginalSlot);
        }
        // C) occupied → swap (with edge‑logic fallback)
        else
        {
            FItem HoveredItem = Items[HoveredIndex];
            Items[HoveredIndex] = DraggedItem;
            UpdateSlotUI(HoveredIndex);

            bool bOrigEdge =
                (OriginalSlot / MaxColumns) == 0 ||
                (OriginalSlot / MaxColumns) == MaxRows - 1 ||
                (OriginalSlot % MaxColumns) == 0 ||
                (OriginalSlot % MaxColumns) == MaxColumns - 1;

            if (!bOrigEdge)
            {
                // interior swap
                Items[OriginalSlot] = HoveredItem;
                UpdateSlotUI(OriginalSlot);
            }
            else
            {
                // edge: find first empty or fallback
                uint32 Empty = FindFirstEmptySlot();
                if (Empty != INDEX_NONE)
                {
                    Items[Empty] = HoveredItem;
                    UpdateSlotUI(Empty);
                }
                else
                {
                    Items[OriginalSlot] = HoveredItem;
                    UpdateSlotUI(OriginalSlot);
                }
            }

            // remove original if it still had the dragged item
            if (Items[OriginalSlot].WorldObjectReference)
            {
                RemoveItem(OriginalSlot);
                UpdateSlotUI(OriginalSlot);
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

        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(
            AStaticMeshActor::StaticClass(), S, P);

        if (Actor)
        {
            Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            if (DraggedItem.StaticMesh.IsValid())
                if (auto* Mesh = DraggedItem.StaticMesh.LoadSynchronous())
                    Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);

        }

        for (int32 i = 0; i < DraggedItem.StoredMaterials.Num(); ++i)
        {
            if (DraggedItem.StoredMaterials[i].IsValid())
            {
                UMaterialInterface* Mat = DraggedItem.StoredMaterials[i].LoadSynchronous();
                if (Mat)
                {
                    Actor->GetStaticMeshComponent()->SetMaterial(i, Mat);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Material %d failed to load."), i);
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Material %d is invalid."), i);
            }
        }


        if (Items[OriginalSlot].WorldObjectReference)
        {
            RemoveItem(OriginalSlot);
        }
    }

    // 5) Finalize
    DragState = EDragState::Released;

    // 6) Refresh visuals
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        UpdateSlotUI(i);
    }

    // Release the mouse capture so other widgets can receive events again
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
    NewItem.WorldObjectReference = ItemActor->GetClass();

    // Store the world transform at the moment of pickup.
    NewItem.WorldObjectTransform = ItemActor->GetActorTransform();

    // Store the index in the inventory.
    int32 MaxExistingIndex = -1;
    for (const FItem& Item : Items)
    {
        if (Item.WorldObjectReference) // or your IsValidItem() check
        {
            MaxExistingIndex = FMath::Max(MaxExistingIndex, Item.Index);
        }
    }
    NewItem.Index = MaxExistingIndex + 1;

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
            UMaterialInterface* Mat = MeshComp->GetMaterial(i);
            if (IsValid(Mat))
            {
                NewItem.StoredMaterials.Add(Mat);
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
    if (!Items.IsValidIndex(SlotIndex) || !Items[SlotIndex].WorldObjectReference)
    {
        UE_LOG(LogTemp, Warning, TEXT("RemoveItem: Invalid slot %d or no item to remove"), SlotIndex);
        return;
    }

    Items[SlotIndex] = FItem();
    UpdateSlotUI(SlotIndex);   // <-- this ensures the slot is cleared visually

    if (ItemCounter > 0) ItemCounter--;

    ItemCounter = 0;
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        if (Items[i].WorldObjectReference)
        {
            Items[i].Index = ItemCounter++;
            CreateItemIcon(i); // Rebuild UI, but don’t change the index
        }
    }
    bIsInventoryFull = (FindFirstEmptySlot() == INDEX_NONE);
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
    if (BackgroundBorder) BackgroundBorder->ForceLayoutPrepass();
    if (Grid) Grid->ForceLayoutPrepass();
    if (Canvas) Canvas->ForceLayoutPrepass();

    for (TObjectPtr<UBorder> Border : ForegroundBorders)
        if (Border) Border->ForceLayoutPrepass();
}

void UInventory::MoveItem(const FPointerEvent& MouseEvent)
{
    if (DragState != EDragState::Moved && DragState != EDragState::Select)
    {
        UE_LOG(LogTemp, Warning, TEXT("MoveItem: Not dragging, exiting"));
        return;
    }

    uint64 HoveredSlot = FindHoveredSlot(MouseEvent);
    UE_LOG(LogTemp, Log, TEXT("MoveItem: HoveredIndex=%d, OriginalSlot=%d, DragStartSlot=%d"),
        HoveredSlot, OriginalSlot, DragStartSlot);

    if (HoveredSlot == INDEX_NONE || !Items.IsValidIndex(HoveredSlot))
    {
        UE_LOG(LogTemp, Warning, TEXT("MoveItem: Invalid HoveredIndex=%d or out of bounds"), HoveredSlot);
        return;
    }

    PreviousSlotIndex = HoveredSlot;

    if (HoveredSlot == OriginalSlot)
    {
        UE_LOG(LogTemp, Log, TEXT("MoveItem: HoveredIndex same as OriginalSlotIndex, skipping"));
        return;
    }

    uint64 FromRow = OriginalSlot / MaxColumns;
    uint64 FromCol = OriginalSlot % MaxColumns;

    uint64 ToRow = HoveredSlot / MaxColumns;
    uint64 ToCol = HoveredSlot % MaxColumns;

    EDirection Direction = GetMoveDirection(FromRow, FromCol, ToRow, ToCol);
    UE_LOG(LogTemp, Log, TEXT("MoveItem: Moving from (%d,%d) to (%d,%d), Direction=%d"),
        FromRow, FromCol, ToRow, ToCol, (uint8)Direction);

    if (Items[HoveredSlot].WorldObjectReference)
    {
        FItem TempItem = Items[HoveredSlot];
        Items[HoveredSlot] = DraggedItem;
        Items[OriginalSlot] = TempItem;
        UpdateSlotUI(OriginalSlot);
        UpdateSlotUI(HoveredSlot);
        UE_LOG(LogTemp, Log, TEXT("MoveItem: Swapped item %d with item in slot %d"), DraggedItem.Index, HoveredSlot);
    }
    else
    {
        Items[HoveredSlot] = DraggedItem;
        Items[OriginalSlot] = FItem();
        UpdateSlotUI(OriginalSlot);
        UpdateSlotUI(HoveredSlot);
        UE_LOG(LogTemp, Log, TEXT("MoveItem: Moved item %d to empty slot %d"), DraggedItem.Index, HoveredSlot);
    }

    DragStartSlot = HoveredSlot;
    OriginalSlot = HoveredSlot;

    if (Grid) Grid->ForceLayoutPrepass();
    if (Canvas) Canvas->ForceLayoutPrepass();
}

EDirection UInventory::GetMoveDirection(uint32 RowA, uint32 ColA, uint32 RowB, uint32 ColB) const
{
    if (RowA == RowB && ColA < ColB) return EDirection::Right;
    if (RowA == RowB && ColA > ColB) return EDirection::Left;
    if (ColA == ColB && RowA < RowB) return EDirection::Down;
    if (ColA == ColB && RowA > RowB) return EDirection::Up;
    return EDirection::Null;
}

void UInventory::UpdateSlotUI(uint32 SlotIndex)
{
    if (!Items.IsValidIndex(SlotIndex) ||
        !ForegroundBorders.IsValidIndex(SlotIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateSlotUI: Invalid Slot %d"), SlotIndex);
        return;
    }

    UBorder* SlotBorder = ForegroundBorders[SlotIndex].Get();
    if (!SlotBorder) return;

    USizeBox* SizeBox = Cast<USizeBox>(SlotBorder->GetContent());
    if (!SizeBox) return;

    SizeBox->ClearChildren();

    // While dragging, leave the original slot blank
    if (DragState == EDragState::Moved &&
        SlotIndex == OriginalSlot)
    {
        return;
    }

    if (Items[SlotIndex].WorldObjectReference)
    {
        CreateItemIcon(SlotIndex);
    }

    SlotBorder->SetVisibility(ESlateVisibility::Visible);
    SlotBorder->ForceLayoutPrepass();

    UE_LOG(LogTemp, Log, TEXT(
        "UpdateSlotUI: Slot %d updated, HasItem=%s"),
        SlotIndex,
        Items[SlotIndex].WorldObjectReference ? TEXT("True") : TEXT("False"));
}

void UInventory::CreateItemIcon(uint32 SlotIndex)
{
    // Validate indices
    if (!Items.IsValidIndex(SlotIndex) || !ForegroundBorders.IsValidIndex(SlotIndex))
    {
        return;
    }

    // Get the SizeBox from the border
    TObjectPtr<USizeBox> SizeBox = Cast<USizeBox>(ForegroundBorders[SlotIndex]->GetContent());
    if (!SizeBox)
    {
        return;
    }

    // Ensure we have a single overlay container
    UOverlay* IconOverlay = Cast<UOverlay>(SizeBox->GetContent());
    if (!IconOverlay)
    {
        IconOverlay = NewObject<UOverlay>(this);
        IconOverlay->SetVisibility(ESlateVisibility::Visible);
        SizeBox->SetContent(IconOverlay);
    }
    else
    {
        // Clear out any old icon/text
        IconOverlay->ClearChildren();
    }

    // --- 1) Create and configure the item icon ---

    UImage* ItemIcon = NewObject<UImage>(this);
    ItemIcon->SetVisibility(ESlateVisibility::Visible);

    // Fill the overlay slot
    if (UOverlaySlot* ImageSlot = IconOverlay->AddChildToOverlay(ItemIcon))
    {
        ImageSlot->SetHorizontalAlignment(HAlign_Fill);
        ImageSlot->SetVerticalAlignment(VAlign_Fill);
    }

        ItemIcon->SetColorAndOpacity(FLinearColor::Blue);

    // --- 2) If the item has a WorldObjectReference, add the counter ---

    if (Items[SlotIndex].WorldObjectReference)
    {
        UTextBlock* CounterText = NewObject<UTextBlock>(this);
        CounterText->SetVisibility(ESlateVisibility::Visible);

        if (UOverlaySlot* TextSlot = IconOverlay->AddChildToOverlay(CounterText))
        {
            TextSlot->SetHorizontalAlignment(HAlign_Center);
            TextSlot->SetVerticalAlignment(VAlign_Center);  // or VAlign_Center if preferred
        }

        CounterText->SetText(FText::AsNumber(Items[SlotIndex].Index));
        CounterText->SetColorAndOpacity(FLinearColor::Red);
        CounterText->SetJustification(ETextJustify::Center);
        CounterText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 20));
    }
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
