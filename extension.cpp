/**
 * =============================================================================
 * SourceMod Blind Hook Extension
 * Copyright (C) 2019 Maxim "Kailo" Telezhenko. All rights reserved.
 * Copyright (C) 2025 InFro. All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 */

#include "extension.h"
#include "subhook/subhook.h"
#include <mathlib/vector.h>
#include <datamap.h>
#include <basehandle.h>

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

BlindHook g_BlindHook;		/**< Global singleton for extension's main interface */

SMEXT_LINK(&g_BlindHook);

IForward *g_pBlindForward = NULL;
static subhook::Hook s_HookPercentageOfFlashForPlayer;

using PercentageOfFlashForPlayerFn = float(__cdecl *)(CBaseEntity *, Vector, CBaseEntity *);
static PercentageOfFlashForPlayerFn PercentageOfFlashForPlayer = NULL;

CBaseEntity *GetThrower(CBaseEntity *pEntity)
{
	datamap_t *pMap = gamehelpers->GetDataMap(pEntity);
	if (!pMap)
	{
		return NULL;
	}

	sm_datatable_info_t info;
	if (!gamehelpers->FindDataMapInfo(pMap, "m_hThrower", &info))
	{
		return NULL;
	}

	CBaseHandle *hndl = reinterpret_cast<CBaseHandle *>(reinterpret_cast<uint8_t *>(pEntity) + info.actual_offset);
	if (!hndl)
	{
		return NULL;
	}

	return gamehelpers->ReferenceToEntity(hndl->GetEntryIndex());
}

float Hooked_PercentageOfFlashForPlayer(CBaseEntity *pEntity, Vector flashPos, CBaseEntity *pevInflictor)
{
	float percentageOfFlash = PercentageOfFlashForPlayer(pEntity, flashPos, pevInflictor);
	if (g_pBlindForward->GetFunctionCount() < 1 || percentageOfFlash <= 0.0f)
	{
		return percentageOfFlash;
	}

	CBaseEntity *pevAttacker = GetThrower(pevInflictor);
	if (!pevAttacker)
	{
		pevAttacker = pevInflictor;
	}

	int attackerIndex = -1;
	if (pevAttacker != pevInflictor)
	{
		attackerIndex = gamehelpers->EntityToBCompatRef(pevAttacker);
	}

	cell_t result = Pl_Continue;
	g_pBlindForward->PushCell(gamehelpers->EntityToBCompatRef(pEntity));
	g_pBlindForward->PushCell(attackerIndex);
	g_pBlindForward->PushCell(gamehelpers->EntityToBCompatRef(pevInflictor));
	g_pBlindForward->Execute(&result);

	if (result == Pl_Handled || result == Pl_Stop)
	{
		return 0.0f;
	}

	return percentageOfFlash;
}

bool BlindHook::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	IGameConfig* pGameConfig = NULL;
	if (!gameconfs->LoadGameConfigFile("blindhook.games", &pGameConfig, error, maxlength))
	{
		return false;
	}

	void *addr;
	if (!pGameConfig->GetMemSig("PercentageOfFlashForPlayer", &addr) || !addr)
	{
		ke::SafeSprintf(error, maxlength, "Failed to get signature for PercentageOfFlashForPlayer");
		return false;
	}

	gameconfs->CloseGameConfigFile(pGameConfig);

	if (!s_HookPercentageOfFlashForPlayer.Install(addr, (void *)Hooked_PercentageOfFlashForPlayer) ||
		!s_HookPercentageOfFlashForPlayer.GetTrampoline())
	{
		ke::SafeSprintf(error, maxlength, "Failed to install hook for PercentageOfFlashForPlayer");
		return false;
	}

	PercentageOfFlashForPlayer = PercentageOfFlashForPlayerFn(s_HookPercentageOfFlashForPlayer.GetTrampoline());
	if(!PercentageOfFlashForPlayer)
	{
		ke::SafeSprintf(error, maxlength, "Failed to get trampoline for PercentageOfFlashForPlayer");
		return false;
	}

	g_pBlindForward = forwards->CreateForward("CS_OnBlindPlayer", ET_Hook, 3, NULL, Param_Cell, Param_Cell, Param_Cell);
	if (!g_pBlindForward)
	{
		ke::SafeSprintf(error, maxlength, "Failed to create forward CS_OnBlindPlayer");
		return false;
	}

	sharesys->RegisterLibrary(myself, "blindhook");

	return true;
}

void BlindHook::SDK_OnUnload()
{
	if (s_HookPercentageOfFlashForPlayer.IsInstalled())
	{
		s_HookPercentageOfFlashForPlayer.Remove();
	}

	forwards->ReleaseForward(g_pBlindForward);
}
