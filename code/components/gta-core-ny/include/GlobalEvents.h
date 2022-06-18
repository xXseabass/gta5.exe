/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#pragma once

#ifndef GTACORE_EXPORT
#ifdef COMPILING_GTA_CORE_NY
#define GTACORE_EXPORT __declspec(dllexport)
#else
#define GTACORE_EXPORT __declspec(dllimport)
#endif
#endif

extern GTACORE_EXPORT fwEvent<> OnMsgConfirm;