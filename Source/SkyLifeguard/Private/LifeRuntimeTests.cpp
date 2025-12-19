#include "LifeRuntimeTests.h"

namespace LifeRuntimeTest
{
	bool LifeRuntimeTest::CheckTagIsA(const FGameplayTag& A, const FGameplayTag& B)
	{
		if (!A.IsValid()) return false;
		return A.MatchesTag(B);
	}
}
