#include "LifeContracts.h"

namespace Debug
{
    /**
     * Helper to check if a single pointer-like property value is valid.

     * @return true if valid (or not a pointer type), false if null/invalid pointer
     */
    static bool IsPointerPropertyValueValid(FProperty* ElementProperty, void const* ElementAddress)
    {
        if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(ElementProperty)) {
            return ObjProp->GetPropertyValue(ElementAddress) != nullptr;
        }
        if (const FWeakObjectProperty* WeakProp = CastField<FWeakObjectProperty>(ElementProperty)) {
            return WeakProp->GetPropertyValue(ElementAddress).IsValid();
        }
        if (const FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(ElementProperty)) {
            return !SoftProp->GetPropertyValue(ElementAddress).IsNull();
        }
        if (const FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(ElementProperty)) {
            return !SoftClassProp->GetPropertyValue(ElementAddress).IsNull();
        }
        if (const FClassProperty* ClassProp = CastField<FClassProperty>(ElementProperty)) {
            return ClassProp->GetPropertyValue(ElementAddress) != nullptr;
        }
        if (const FInterfaceProperty* InterfaceProp = CastField<FInterfaceProperty>(ElementProperty)) {
            const FScriptInterface& Interface = InterfaceProp->GetPropertyValue(ElementAddress);
            return Interface.GetObject() != nullptr;
        }
        // Not a pointer type - considered valid
        return true;
    }

	/**
     * Check if the element property is a pointer-like type that we should validate.
     */
    static bool IsPointerLikeProperty(FProperty* Property)
    {
        return CastField<FObjectProperty>(Property) != nullptr ||
               CastField<FWeakObjectProperty>(Property) != nullptr ||
               CastField<FSoftObjectProperty>(Property) != nullptr ||
               CastField<FSoftClassProperty>(Property) != nullptr ||
               CastField<FClassProperty>(Property) != nullptr ||
               CastField<FInterfaceProperty>(Property) != nullptr;
    }


	/**
     * Validates a container for MemSafeContainer invariant.
     * @return true if valid, false if any element is an invalid pointer
     */
    static bool ValidateMemSafeContainer(FProperty* Property, void const* PropertyAddress, const FString& ClassName, const FString& PropertyName)
    {
        // TArray
        if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property)) {
            FProperty* InnerProp = ArrayProp->Inner;
            
            // If inner type is not pointer-like, container is valid
            if (!IsPointerLikeProperty(InnerProp)) {
                return true;
            }

            FScriptArrayHelper ArrayHelper(ArrayProp, PropertyAddress);
            for (int32 i = 0; i < ArrayHelper.Num(); ++i)
            {
                void const* ElementPtr = ArrayHelper.GetRawPtr(i);
                if (!IsPointerPropertyValueValid(InnerProp, ElementPtr)) {
                    return false;
                }
            }
            return true;
        }

        // TSet
        if (const FSetProperty* SetProp = CastField<FSetProperty>(Property)) {
            FProperty* ElementProp = SetProp->ElementProp;
            
            if (!IsPointerLikeProperty(ElementProp)) {
                return true;
            }

            FScriptSetHelper SetHelper(SetProp, PropertyAddress);
            for (int32 i = 0; i < SetHelper.Num(); ++i)
            {
                if (SetHelper.IsValidIndex(i))
                {
                    void const* ElementPtr = SetHelper.GetElementPtr(i);
                    if (!IsPointerPropertyValueValid(ElementProp, ElementPtr)) {
                        return false;
                    }
                }
            }
            return true;
        }

        // TMap
        if (const FMapProperty* MapProp = CastField<FMapProperty>(Property)) {
            FProperty* KeyProp = MapProp->KeyProp;
            FProperty* ValueProp = MapProp->ValueProp;
            
            const bool bKeyIsPointer = IsPointerLikeProperty(KeyProp);
            const bool bValueIsPointer = IsPointerLikeProperty(ValueProp);
            
            if (!bKeyIsPointer && !bValueIsPointer) {
                return true;
            }

            FScriptMapHelper MapHelper(MapProp, PropertyAddress);
            for (int32 i = 0; i < MapHelper.Num(); ++i)
            {
                if (MapHelper.IsValidIndex(i))
                {
                    if (bKeyIsPointer)
                    {
                        void const* KeyPtr = MapHelper.GetKeyPtr(i);
                        if (!IsPointerPropertyValueValid(KeyProp, KeyPtr)) {
                            return false;
                        }
                    }
                    if (bValueIsPointer)
                    {
                        void const* ValuePtr = MapHelper.GetValuePtr(i);
                        if (!IsPointerPropertyValueValid(ValueProp, ValuePtr)) {
                            return false;
                        }
                    }
                }
            }
            return true;
        }

        // TOptional
        if (const auto* const OptProp = CastField<FOptionalProperty>(Property)) {
            FProperty* ValueProperty = OptProp->GetValueProperty();
        	const auto bIsSet = OptProp->IsSet(PropertyAddress);
        	
        	if (!bIsSet) {
        		// Unset optional is memory safe since it's not set to anything
        		return true;
        	}
        	
        	// Optional is set --

			// If the value type is not pointer-like, container is valid
            if (!IsPointerLikeProperty(ValueProperty)) {
                return true;
            }
        	
        	// Check if the optional has a value
        	const void* ValueAddress = OptProp->GetValuePointerForRead(PropertyAddress);
        	if (ValueAddress == nullptr) {
        		return false;
        	}

            return true;
        }

        // Not a recognized container type
        return false;
    }

	void Debug::CheckClassInvariants(const UObject* Object)
	{
		LG_PRECOND(Object);

		UClass* Class = Object->GetClass();
		const FString ClassName = Class->GetName();

		for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt) {
			FProperty* Property = *PropIt;
			if (!Property->HasMetaData(TEXT("Invariant"))) {
				continue;
			}

			const FString InvariantRule = Property->GetMetaData(TEXT("Invariant"));
			const FString PropertyName = Property->GetName();
			void const* PropertyAddress = Property->ContainerPtrToValuePtr<void>(Object);

			// Invariant=MemSafe
			if (InvariantRule == TEXT("MemSafe")) {
				if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property)) {
					const auto Value = ObjectProperty->GetPropertyValue_InContainer(Object);
					checkf(Value != nullptr, TEXT("Invariant=MemSafe violation on %s::%s"), *ClassName, *PropertyName)
				} else if (const FClassProperty* ClassProperty = CastField<FClassProperty>(Property)) {
					const auto Value = ClassProperty->GetPropertyValue_InContainer(Object);
					checkf(Value != nullptr, TEXT("Invariant=MemSafe violation on %s::%s"), *ClassName, *PropertyName)
				} else if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property)) {
					const auto Value = SoftObjectProperty->GetPropertyValue_InContainer(Object);
					checkf(Value != nullptr, TEXT("Invariant=MemSafe violation on %s::%s"), *ClassName, *PropertyName)
				} else if (const FWeakObjectProperty* WeakObjectProperty = CastField<FWeakObjectProperty>(Property)) {
					const FWeakObjectPtr Value = WeakObjectProperty->GetPropertyValue_InContainer(Object);
					checkf(Value != nullptr, TEXT("Invariant=MemSafe violation on %s::%s"), *ClassName, *PropertyName)
				} else if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(Property)) {
					const auto Value = SoftClassProperty->GetPropertyValue_InContainer(Object);
					checkf(Value != nullptr, TEXT("Invariant=MemSafe violation on %s::%s"), *ClassName, *PropertyName)
				} else if (const FInterfaceProperty* InterfaceProperty = CastField<FInterfaceProperty>(Property)) {
                    const FScriptInterface& Value = InterfaceProperty->GetPropertyValue(PropertyAddress);
                    checkf(Value.GetObject() != nullptr, TEXT("Invariant=MemSafe violation on %s::%s"), *ClassName, *PropertyName)
                } else {
					checkf(false, TEXT("Invariant=MemSafe used on non-pointer property %s::%s"), *ClassName, *PropertyName);
				}
			}
			// Invariant=MemSafeContainer
            else if (InvariantRule == TEXT("MemSafeContainer")) {
                const bool bIsContainer = CastField<FArrayProperty>(Property) != nullptr ||
                                          CastField<FSetProperty>(Property) != nullptr ||
                                          CastField<FMapProperty>(Property) != nullptr ||
                                          CastField<FOptionalProperty>(Property) != nullptr;
                
                checkf(bIsContainer, TEXT("Invariant=MemSafeContainer used on non-container property %s::%s"), *ClassName, *PropertyName);
                
                const bool bIsValid = ValidateMemSafeContainer(Property, PropertyAddress, ClassName, PropertyName);
                checkf(bIsValid, TEXT("Invariant=MemSafeContainer violation on %s::%s (container has null/invalid pointer element)"), *ClassName, *PropertyName);
            }
			// Invariant=ID
			else if (InvariantRule == TEXT("ID")) {
				const bool bIsValid = TestIntegerProperty(Property, PropertyAddress, [](auto Value){ return Value != INDEX_NONE; });
				checkf(bIsValid, TEXT("Invariant=ID violation on %s::%s"), *ClassName, *PropertyName);
			}
			// Invariant=Gte0 (Greater than or equal to 0)
			else if (InvariantRule == TEXT("Gte0")) {
				const bool bIsValid = TestArithmeticProperty(Property, PropertyAddress, [](auto Value){ return Value >= 0; });
				checkf(bIsValid, TEXT("Invariant=Gte0 violation on %s::%s"), *ClassName, *PropertyName);
			}
			// Invariant=Gt0 (Greater than 0)
			else if (InvariantRule == TEXT("Gt0")) {
				const bool bIsValid = TestArithmeticProperty(Property, PropertyAddress, [](auto Value){ return Value > 0; });
				checkf(bIsValid, TEXT("Invariant=Gt0 violation on %s::%s"), *ClassName, *PropertyName);
			}
			// Invariant=Lte0 (Less than or equal to 0)
			else if (InvariantRule == TEXT("Lte0")) {
				const bool bIsValid = TestArithmeticProperty(Property, PropertyAddress, [](auto Value){ return Value <= 0; });
				checkf(bIsValid, TEXT("Invariant=Lte0 violation on %s::%s"), *ClassName, *PropertyName);
			}
			// Invariant=Lt0 (Less than 0)
			else if (InvariantRule == TEXT("Lt0")) {
				const bool bIsValid = TestArithmeticProperty(Property, PropertyAddress, [](auto Value){ return Value < 0; });
				checkf(bIsValid, TEXT("Invariant=Lt0 violation on %s::%s"), *ClassName, *PropertyName);
			}
			// Invariant=Range[lower,upper] or Range(lower,upper] etc.
			else if (InvariantRule.StartsWith(TEXT("Range")))
			{
				// Expect syntax Range[lo,hi], Range(lo,hi], etc. but also allow for Range (1, 2)
				FString RangeSpec = InvariantRule.Mid(5).TrimStart(); // skip "Range", trim space
				if (RangeSpec.IsEmpty() || (RangeSpec[0] != '(' && RangeSpec[0] != '[')) {
					checkNoEntry()
				}
                
				checkf(RangeSpec.Len() >= 2, TEXT("Invalid Range invariant format on %s::%s"), *ClassName, *PropertyName);

				const TCHAR LowerBracket = RangeSpec[0];
				const TCHAR UpperBracket = RangeSpec[RangeSpec.Len() - 1];
				const bool bLowerInclusive = (LowerBracket == TEXT('['));
				const bool bUpperInclusive = (UpperBracket == TEXT(']'));

				// inner content between brackets
				const FString Inner = RangeSpec.Mid(1, RangeSpec.Len() - 2);
				TArray<FString> Parts;
				Inner.ParseIntoArray(Parts, TEXT(","), true);
				checkf(Parts.Num() == 2, TEXT("Range invariant must contain two comma-separated bounds on %s::%s"), *ClassName, *PropertyName);

				FString LowerTrimmed = *Parts[0].TrimStartAndEnd();
				FString UpperTrimmed = *Parts[1].TrimStartAndEnd();
                
				// Sanitize bounds: remove whitespace/newlines/tabs and common thousands separators,
				// keep only digits, sign, decimal point and exponent markers.
				auto SanitizeNumberString = [](const FString& In) -> FString {
					FString Out;
					Out.Reserve(In.Len());
					for (const TCHAR C : In) {
						if (FChar::IsWhitespace(C)) continue; // spaces, tabs, newlines, etc
						if (C == ',' || C == '_') continue;  // common thousands separators
						if (FChar::IsDigit(C) || C == '.' || C == '+' || C == '-' || C == 'e' || C == 'E') {
							Out.AppendChar(C);
						}
						// drop anything else
					}
					return Out;
				};

				// Overwrite the parsed trimmed strings with sanitized versions so subsequent Atod() calls
				// receive clean numeric text.
				LowerTrimmed = SanitizeNumberString(LowerTrimmed);
				UpperTrimmed = SanitizeNumberString(UpperTrimmed);

                // If property is integral, do integer comparisons (avoid casting integers to double).
                bool bIsValid = false;

                const bool bBoundsHaveFloatMarker =
                    LowerTrimmed.Contains(TEXT(".")) || LowerTrimmed.Contains(TEXT("e")) || LowerTrimmed.Contains(TEXT("E")) ||
                    UpperTrimmed.Contains(TEXT(".")) || UpperTrimmed.Contains(TEXT("e")) || UpperTrimmed.Contains(TEXT("E"));

                // Integer property path
                if (CastField<FInt8Property>(Property)   || CastField<FInt16Property>(Property) ||
                    CastField<FIntProperty>(Property)    || CastField<FInt64Property>(Property) ||
                    CastField<FByteProperty>(Property)  || CastField<FUInt16Property>(Property) ||
                    CastField<FUInt32Property>(Property)|| CastField<FUInt64Property>(Property))
                {
                    // Do not allow floating-point bounds for integer properties — require integer bounds.
                    if (bBoundsHaveFloatMarker) {
                        checkf(false, TEXT("Range invariant for integer property must use integer bounds on %s::%s"), *ClassName, *PropertyName);
                    }

                    bIsValid = TestIntegerProperty(Property, PropertyAddress, [LowerTrimmed, UpperTrimmed, bLowerInclusive, bUpperInclusive]<typename T>(T V) -> bool {
                        if constexpr (std::is_signed_v<T>) {
                        	const int64 LowerI = FCString::Atoi64(*LowerTrimmed);
							const int64 UpperI = FCString::Atoi64(*UpperTrimmed);
                        	
							checkf(LowerI <= UpperI, TEXT("Range invariant lower bound must be <= upper bound"));
                        	
                            const T LowerBound = static_cast<T>(LowerI);
                            const T UpperBound = static_cast<T>(UpperI);
                            const bool LowerOk = bLowerInclusive ? (V >= LowerBound) : (V > LowerBound);
                            const bool UpperOk = bUpperInclusive ? (V <= UpperBound) : (V < UpperBound);
                            return LowerOk && UpperOk;
                        } else {
                        	const uint64 LowerI = FCString::Strtoui64(*LowerTrimmed, nullptr, 10);
                        	const uint64 UpperI = FCString::Strtoui64(*UpperTrimmed, nullptr, 10);
                        	
                            const T LowerBound = static_cast<T>(LowerI);
                            const T UpperBound = static_cast<T>(UpperI);
                            const bool LowerOk = bLowerInclusive ? (V >= LowerBound) : (V > LowerBound);
                            const bool UpperOk = bUpperInclusive ? (V <= UpperBound) : (V < UpperBound);
                            return LowerOk && UpperOk;
                        }
                    });
                }
                // Floating point property path
                else if (CastField<FFloatProperty>(Property) || CastField<FDoubleProperty>(Property)) {
                    const double LowerD = FCString::Atod(*LowerTrimmed);
                    const double UpperD = FCString::Atod(*UpperTrimmed);
                    checkf(LowerD <= UpperD, TEXT("Range invariant lower bound must be <= upper bound on %s::%s"), *ClassName, *PropertyName);

                    bIsValid = TestArithmeticProperty(Property, PropertyAddress, [LowerD, UpperD, bLowerInclusive, bUpperInclusive](auto V) -> bool {
                        const double Value = static_cast<double>(V);
                   		// Use a small relative epsilon scaled by value/bounds to tolerate tiny FP error,
                        // and apply it outward (lower - eps, upper + eps) so we don't accidentally exclude valid values.
                        const double Scale = FMath::Max(1.0, FMath::Max(FMath::Abs(LowerD), FMath::Max(FMath::Abs(UpperD), FMath::Abs(Value))));
                        const double Eps = FMath::Clamp(Scale * 1e-6, 1e-10, 1e-3);
                   		const bool LowerOk = bLowerInclusive ? (Value >= (LowerD - Eps)) : (Value > (LowerD - Eps));
                        const bool UpperOk = bUpperInclusive ? (Value <= (UpperD + Eps)) : (Value < (UpperD + Eps));
                        return LowerOk && UpperOk;
                    });
                }
				else {
                    checkf(false, TEXT("Range invariant used on non-arithmetic property %s::%s"), *ClassName, *PropertyName);
                }

				checkf(bIsValid, TEXT("Range invariant violation on %s::%s"), *ClassName, *PropertyName);
			}
			// Invariant=name
			else if (InvariantRule == TEXT("name")) {
				if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property)) {
					const FName Value = NameProperty->GetPropertyValue(PropertyAddress);
					checkf(!Value.IsNone(), TEXT("Invariant=name violation on %s::%s"), *ClassName, *PropertyName);
				} else {
					checkf(false, TEXT("Invariant=name used on non-FName property %s::%s"), *ClassName, *PropertyName);
				}
			}
			// Invariant=True
			else if (InvariantRule == TEXT("True")) {
				if (const FBoolProperty* NameProperty = CastField<FBoolProperty>(Property)) {
					const bool Value = NameProperty->GetPropertyValue(PropertyAddress);
					checkf(Value, TEXT("Invariant=True violation on %s::%s"), *ClassName, *PropertyName);
				} else {
					checkf(false, TEXT("Invariant=True used on non-bool property %s::%s"), *ClassName, *PropertyName);
				}
			}
			else if (InvariantRule == TEXT("False")) {
				if (const FBoolProperty* NameProperty = CastField<FBoolProperty>(Property)) {
					const bool Value = NameProperty->GetPropertyValue(PropertyAddress);
					checkf(!Value, TEXT("Invariant=False violation on %s::%s"), *ClassName, *PropertyName);
				} else {
					checkf(false, TEXT("Invariant=False used on non-bool property %s::%s"), *ClassName, *PropertyName);
				}
			}
			// Invariant=Contract*
			else if (InvariantRule == TEXT("Contract*")) {
				if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property)) {
					const auto Value = ObjectProperty->GetObjectPropertyValue(PropertyAddress);
					LG_CONTRACT_CHECK_MSG(Value->GetClass() != Object->GetClass(), "An object cannot contain an invariant member of the same class, as that would imply an infinite loop of invariants");
					checkf(IsValid(Value), TEXT("Invariant=Contract* violation on %s::%s"), *ClassName, *PropertyName);
					if (Value) {
						// Recursive check
						CheckClassInvariants(Value);
					}
				} else {
					checkf(false, TEXT("Invariant=Contract* used on non-pointer property %s::%s"), *ClassName, *PropertyName);
				}
			}
			// Invariant=PublicFunctionName
			else {
				UFunction* Function = Class->FindFunctionByName(*InvariantRule);
				checkf(Function != nullptr, TEXT("Invariant function '%s' not found."), *InvariantRule);

				if (Function) {
					// Check for const and no parameters
					checkf(Function->HasAnyFunctionFlags(FUNC_Const), TEXT("Invariant function '%s' must be const."), *InvariantRule);
					checkf(Function->NumParms == 1, TEXT("Invariant function '%s' must have exactly one return value (bool) and no parameters."), *InvariantRule);

					FStructOnScope FuncParams(Function);
					const_cast<UObject*>(Object)->ProcessEvent(Function, FuncParams.GetStructMemory());

					FBoolProperty* ReturnProp = CastField<FBoolProperty>(Function->GetReturnProperty());
					checkf(ReturnProp != nullptr, TEXT("Invariant function '%s' must return a bool."), *InvariantRule);

					if(ReturnProp) {
						const bool bIsValid = ReturnProp->GetPropertyValue(FuncParams.GetStructMemory());
						checkf(bIsValid, TEXT("Invariant violation on %s::%s. Custom check '%s' failed."), *ClassName, *PropertyName, *InvariantRule);
					}
				}
			}
		}
		for (TFieldIterator<UFunction> FuncIt(Class); FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;
			if (Function->HasMetaData(TEXT("Invariant")))
			{
				const FString FunctionName = Function->GetName();

				// Check for const, no parameters, and bool return type
				checkf(Function->HasAnyFunctionFlags(FUNC_Const), TEXT("Invariant function '%s' on class '%s' must be const."), *FunctionName, *ClassName);
				checkf(Function->NumParms == 1, TEXT("Invariant function '%s' on class '%s' must have no parameters and return a bool."), *FunctionName, *ClassName);

				FBoolProperty* ReturnProp = CastField<FBoolProperty>(Function->GetReturnProperty());
				checkf(ReturnProp != nullptr, TEXT("Invariant function '%s' on class '%s' must return a bool."), *FunctionName, *ClassName);

				if (ReturnProp)
				{
					FStructOnScope FuncParams(Function);
					const_cast<UObject*>(Object)->ProcessEvent(Function, FuncParams.GetStructMemory());

					const bool bIsValid = ReturnProp->GetPropertyValue(FuncParams.GetStructMemory());
					checkf(bIsValid, TEXT("Invariant violation: Custom check function '%s' on class '%s' failed."), *FunctionName, *ClassName);
				}
			}
		}
	}
}