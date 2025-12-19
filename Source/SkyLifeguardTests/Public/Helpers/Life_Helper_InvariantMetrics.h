#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Life_Helper_InvariantMetrics.generated.h"

UCLASS()
class ULifeTestInvariantPerfObj : public UObject
{
	GENERATED_BODY()

public:
    // 50 Integers
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int00 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int01 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int02 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int03 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int04 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int05 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int06 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int07 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int08 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int09 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int10 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int11 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int12 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int13 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int14 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int15 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int16 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int17 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int18 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int19 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int20 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int21 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int22 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int23 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int24 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int25 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int26 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int27 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int28 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int29 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int30 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int31 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int32 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int33 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int34 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int35 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int36 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int37 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int38 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int39 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int40 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int41 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int42 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int43 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int44 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int45 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int46 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int47 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int48 = 1;
    UPROPERTY(meta = (Invariant = "Gte0")) int32 Int49 = 1;

    // 25 Pointers
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr00 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr01 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr02 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr03 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr04 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr05 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr06 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr07 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr08 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr09 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr10 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr11 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr12 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr13 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr14 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr15 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr16 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr17 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr18 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr19 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr20 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr21 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr22 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr23 = nullptr;
    UPROPERTY(meta = (Invariant = "MemSafe")) UObject* Ptr24 = nullptr;
};
