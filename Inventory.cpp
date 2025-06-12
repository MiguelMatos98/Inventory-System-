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
      DraggedItemIndex(INDEX_NONE),
      OriginalSlotIndex(INDEX_NONE),
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
    BackgroundBorder_S->SetOffsets(FMargin(-10.0f, 11.0f, 485.0f, 419.0f));

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

            // Ready to pop out as soon as the cursor leaves
            DragState = EDragState::Select;
            MousePosition = InMouseEvent.GetScreenSpacePosition();

            return FReply::Handled();
        }
    }
    return FReply::Unhandled();
}

FReply UInventory::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Only care if we’re “about to pop” or “already popped”
    if (DragState != EDragState::Select
        && DragState != EDragState::Moved)
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

    if (UBorder* Border = ForegroundBorders[DraggedItemIndex].Get())
    {
        const FGeometry SlotGeom = Border->GetCachedGeometry();
        const FVector2D TL = SlotGeom.LocalToAbsolute(FVector2D::ZeroVector);
        const FVector2D BR = TL + SlotGeom.GetLocalSize();
        const int32 Row = DraggedItemIndex / MaxColumns;
        const int32 Col = DraggedItemIndex % MaxColumns;
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
                if (DraggedItem.Texture.IsValid())
                    Img->SetBrushFromTexture(DraggedItem.Texture.Get());
                else
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
    // Only finalize once popped out
    if (DragState != EDragState::Moved)
        return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);

    // 1) Prepass for accurate hit tests
    if (Canvas) Canvas->ForceLayoutPrepass();
    if (Grid)   Grid->ForceLayoutPrepass();
    for (auto& Ptr : ForegroundBorders)
        if (auto* B = Ptr.Get()) B->ForceLayoutPrepass();

    const FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();
    const uint32    HoveredIndex = FindHoveredItemIndex(InMouseEvent);

    // 2) Tear down the ghost
    if (DraggedItemWidget && Canvas)
    {
        Canvas->RemoveChild(DraggedItemWidget);
        DraggedItemWidget = nullptr;
    }

    // 3) If dropped on a valid slot…
    if (HoveredIndex != INDEX_NONE && Items.IsValidIndex(HoveredIndex))
    {
        // A) empty slot → move
        if (!Items[HoveredIndex].WorldObjectReverence)
        {
            Items[HoveredIndex] = DraggedItem;
            UpdateSlotUI(HoveredIndex);

            // only remove original if it still holds something
            if (Items[OriginalSlotIndex].WorldObjectReverence)
            {
                RemoveItem(OriginalSlotIndex);
                UpdateSlotUI(OriginalSlotIndex);
            }
        }
        // B) same slot → restore
        else if (HoveredIndex == OriginalSlotIndex)
        {
            Items[OriginalSlotIndex] = DraggedItem;
            UpdateSlotUI(OriginalSlotIndex);
        }
        // C) occupied → swap (with edge logic)
        else
        {
            FItem Temp = Items[HoveredIndex];
            Items[HoveredIndex] = DraggedItem;
            UpdateSlotUI(HoveredIndex);

            bool bOrigEdge =
                (OriginalSlotIndex / MaxColumns) == 0 ||
                (OriginalSlotIndex / MaxColumns) == MaxRows - 1 ||
                (OriginalSlotIndex % MaxColumns) == 0 ||
                (OriginalSlotIndex % MaxColumns) == MaxColumns - 1;

            if (!bOrigEdge)
            {
                // interior swap
                Items[OriginalSlotIndex] = Temp;
                UpdateSlotUI(OriginalSlotIndex);
            }
            else
            {
                // edge: find first empty or fallback
                uint32 Empty = FindFirstEmptySlot();
                if (Empty != INDEX_NONE)
                {
                    Items[Empty] = Temp;
                    UpdateSlotUI(Empty);
                }
                else
                {
                    Items[OriginalSlotIndex] = Temp;
                    UpdateSlotUI(OriginalSlotIndex);
                }
            }

            // remove original if it still had the dragged item
            if (Items[OriginalSlotIndex].WorldObjectReverence)
            {
                RemoveItem(OriginalSlotIndex);
                UpdateSlotUI(OriginalSlotIndex);
            }
        }
    }
    // 4) Otherwise, drop off-grid → spawn actor
    else if (UWorld* World = GetWorld())
    {
        const FTransform& T = DraggedItem.WorldObjectTransform;
        DrawDebugSphere(World, T.GetLocation(), 25.f, 12, FColor::Blue, false, 5.f);

        FTransform S = T; S.SetScale3D(T.GetScale3D());
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

        // remove original if still present
        if (Items[OriginalSlotIndex].WorldObjectReverence)
        {
            RemoveItem(OriginalSlotIndex);
            UpdateSlotUI(OriginalSlotIndex);
        }
    }

    // 5) Finalize
    DragState = EDragState::Released;

    // 6) Refresh all slots
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        UpdateSlotUI(i);
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
    if (DragState != EDragState::Moved && DragState != EDragState::Select)
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
        SlotIndex == OriginalSlotIndex)
    {
        return;
    }

    if (Items[SlotIndex].WorldObjectReverence)
    {
        CreateItemIcon(SlotIndex);
        CreateIconCounterText(SlotIndex);
    }

    SlotBorder->SetVisibility(ESlateVisibility::Visible);
    SlotBorder->ForceLayoutPrepass();

    UE_LOG(LogTemp, Log, TEXT(
        "UpdateSlotUI: Slot %d updated, HasItem=%s"),
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
