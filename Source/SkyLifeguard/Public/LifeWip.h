#pragma once

#include "LifeLogChannels.h"

/*
 * TO_DO("msg") emits a runtime UE_LOG warning with function/file/line. Use with a string literal: 
 * TO_DO("Finish implementing").
 */
#define TODO(msg) \
		UE_LOG(LogLife, Warning, TEXT("TODO: %s -- %hs @ %hs:%d"), TEXT(msg), __FUNCTION__, __FILE__, __LINE__);

#define TODO_FN \
	TODO(__FUNCTION__)