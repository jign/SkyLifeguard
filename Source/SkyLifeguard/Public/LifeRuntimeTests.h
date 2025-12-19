#pragma once

#include "CoreMinimal.h"
#include "LifeContracts.h"
#include "GameplayTagContainer.h"

#if DO_CHECK
#define TEST_TAG_ISA(Tag, Parent) {TEST_CHECK(LifeRuntimeTest::CheckTagIsA(Tag, Parent));}
#else
#define TEST_TAG_ISA(Tag, Parent)
#endif


namespace LifeRuntimeTest
{
	/**
	 * Checks if tag A is of kind B, for example A.B.C must be either A or A.B (or A.B.C) 
	 */
	SKYLIFEGUARD_API bool CheckTagIsA(const FGameplayTag& A, const FGameplayTag& B);
}
