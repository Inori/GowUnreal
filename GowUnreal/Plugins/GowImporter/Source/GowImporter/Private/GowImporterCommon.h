#pragma once

#include "Logging/LogMacros.h"

/** Declares a log category for this module. */
DEFINE_LOG_CATEGORY_STATIC(LogGowImporterPlugin, Log, All);

#define LOG_DEBUG(format, ...) UE_LOG(LogGowImporterPlugin, Display, TEXT(format), __VA_ARGS__)