#pragma once

#include "Misc/ScopeExit.h"

#define TEST_CHECK(Expr) checkf((Expr), TEXT("Test failure: [%s] @ [%hs:%d]"), TEXT(#Expr), __FUNCTION__, __LINE__)
#define CONTRACT_CHECK(Expr) checkf((Expr), TEXT("Architecture violation: [%s] @ [%hs:%d]"), TEXT(#Expr), __FUNCTION__, __LINE__)
#define LG_CONTRACT_CHECK_MSG(Expr, Msg) checkf((Expr), TEXT("Contract violation (%s): [%s] @ [%hs:%d]"), *FString(Msg), TEXT(#Expr), __FUNCTION__, __LINE__)

/**
 * Check a precondition: something that should be true at the beginning of some scope.
 */
#define PRECOND(Expr) LG_CONTRACT_CHECK_MSG(Expr, "Precondition")
/**
 * Check a postcondition: something that should be true at the end of some scope.
 */
#define POSTCOND(Expr) LG_CONTRACT_CHECK_MSG(Expr, "Postcondition")

/**
 * POSTCOND_SCOPE
 * 
 * * Declares a postcondition that will be checked automatically when the current 
 * scope exits (via return, break, or exception).
 * 
 * * Note: This captures context by reference. It checks the value of 'Expr' 
 * at the MOMENT OF EXIT, not at the moment of declaration.
 */
#define SCOPE_POSTCOND(Expr) \
	ON_SCOPE_EXIT \
	{ \
		POSTCOND(Expr); \
	};

/**
 * Check an invariant for some object
 * @param Expr The UObject for which we want to check all the invariants.
 */
#define INVARIANT(Expr) LG_CONTRACT_CHECK_MSG(Expr, "Invariant")
/**
 * Check an architectural condition. Something that was promised to us by some third party (like Epic when using engine
 * classes).
 *
 * Not a part of the traditional DbC paradigm but used to check things we have no control over but should be true.
 */
#define ARCHCOND(Expr) LG_CONTRACT_CHECK_MSG(Expr, "Architecture")

#if DO_CHECK
/**
 * Checks if the given object passes the class invariants checks. A class invariant is any member UPROPERTY that
 * must be valid for the object to be in a valid state. Not all members of a class with a contract may be invariant,
 * as it may be perfectly valid for some to be null or in an unset state.
 *
 * All properties with the tag meta=(Invariant) must be valid. Invariants can be of the following types. If you need
 * custom validity checks use custom invariant functions.
 *
 * - Invariant=MemSafe -			Pointers of all kinds: Weak Pointers, Soft Pointers, Class Pointers, 
 *										Interface Pointers point to a valid address.
 * - Invariant=MemSafeContainer -	Container (TOptional, TArray, TMap, TSet), where if it contains pointer value(s), 
 *										those values must be valid. Supports: TArray<T*>, TSet<T*>, TMap<K,V> (checks 
 *										both K and V if pointers), TOptional<T*>
 *										Also supports TWeakObjectPtr, TSoftObjectPtr, TSubclassOf, TScriptInterface
 *										as element types.
 * - Invariant=ID -					Integer IDs whose value cannot be INDEX_NONE
 * - Invariant=Gte0 -				Numeric greater than or equal to 0
 * - Invariant=Gt0 -				Numeric greater than 0
 * - Invariant=Lte0 -				Numeric less than or equal to 0
 * - Invariant=Lt0 -				Numeric less than 0
 * - Invariant=Range[a,b] -			Numeric in range. Can mix () and [] for inclusive/exclusive bound checks
 * - Invariant=Name -				FNames that cannot be NAME_NONE
 * - Invariant=True -				Must be true (bool)
 * - Invariant=False -				Must be false (bool)
 *
 * For nonstandard properties (like structs, or custom objects), we offer two methods
 *
 * - Invariant=FunctionName - with signature bool FunctionName() const inside the class
 * - Invariant=Contract* - a pointer which must be valid, and which must also pass invariant validation
 *
 * Be careful with Invariant=Contract*. If you have the same class requiring the same class you get into an infinite
 * loop, but also if you get dupes at any level of the hierarchy so A - requires not null A = infinite loop, but also
 * A - requires not null B (that requires not null A) = also infinite loop.
 * This is not an error in the validation layer! This is an actual logic error. If any hierarchy has at least one loop
 * of MemSafe invariants then the invariants cannot possibly hold. Any class hierarchy that has a loop must have a
 * non-invariant, maybe null reference.
 * 
 * On a 10-year old Intel i7, the average time per full class invariant check on an object with 75 invariants
 * is 0.000024s or 24 microseconds.
 *
 * @param Object - The object with an invariant contract. 
 */
#define CLASS_INVARIANTS(Object) Debug::CheckClassInvariants(Object);
#else
#define CLASS_INVARIANTS(Object)
#endif

/**
 * Will first check if the object is valid, then run a full class invariants check on the object.
 * 
 * @param Object A pointer to check
 */
#define PRECOND_DEEPCHK(Object) \
	PRECOND(Object) \
	CLASS_INVARIANTS(Object)

namespace Debug
{
    /**
     * Helper to test a property of any integer type against a predicate.
     * @param Property The property to test.
     * @param PropertyAddress The memory address of the property's value.
     * @param Predicate A lambda or function object that takes an integer value and returns a bool.
     * @return True if the property is an integer type and the predicate returns true, false otherwise.
     */
    template<typename TPredicate>
    bool TestIntegerProperty(FProperty* Property, void const* PropertyAddress, TPredicate Predicate)
    {
        if (const auto P = CastField<FInt8Property>(Property))   { return Predicate(P->GetPropertyValue(PropertyAddress)); }
        if (const auto P = CastField<FInt16Property>(Property))  { return Predicate(P->GetPropertyValue(PropertyAddress)); }
        if (const auto P = CastField<FIntProperty>(Property))    { return Predicate(P->GetPropertyValue(PropertyAddress)); }
        if (const auto P = CastField<FInt64Property>(Property))  { return Predicate(P->GetPropertyValue(PropertyAddress)); }
        if (const auto P = CastField<FByteProperty>(Property))   { return Predicate(P->GetPropertyValue(PropertyAddress)); }
        if (const auto P = CastField<FUInt16Property>(Property)) { return Predicate(P->GetPropertyValue(PropertyAddress)); }
        if (const auto P = CastField<FUInt32Property>(Property)){ return Predicate(P->GetPropertyValue(PropertyAddress)); }
        if (const auto P = CastField<FUInt64Property>(Property)){ return Predicate(P->GetPropertyValue(PropertyAddress)); }
        return false;
    }

    /**
     * Helper to test a property of any arithmetic type (integer or float) against a predicate.
     * @param Property The property to test.
     * @param PropertyAddress The memory address of the property's value.
     * @param Predicate A lambda or function object that takes a numeric value and returns a bool.
     * @return True if the property is an arithmetic type and the predicate returns true, false otherwise.
     */
    template<typename TPredicate>
    bool TestArithmeticProperty(FProperty* Property, void const* PropertyAddress, TPredicate Predicate)
    {
        if (TestIntegerProperty(Property, PropertyAddress, Predicate)) {
            return true;
        }
        if (const auto P = CastField<FFloatProperty>(Property)) { return Predicate(P->GetPropertyValue(PropertyAddress)); }
        if (const auto P = CastField<FDoubleProperty>(Property)){ return Predicate(P->GetPropertyValue(PropertyAddress)); }
        return false;
    }    

    /**
     * Iterate through all UPROPERTY and UFUNCTION fields. If any is marked meta=(Invariant) then check it.
     * 
     * @param Object The object for which we want to check the invariants
     */
    SKYLIFEGUARD_API void CheckClassInvariants(const UObject* Object);
}

