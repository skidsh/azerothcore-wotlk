/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
* @file cs_deserter.cpp
* @brief .deserter related commands
*
* This file contains the CommandScripts for all deserter sub-commands
*/

#include "Chat.h"
#include "Language.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"

using namespace Acore::ChatCommands;

enum Spells
{
    LFG_SPELL_DUNGEON_DESERTER = 71041,
    BG_SPELL_DESERTER = 26013
};

class deserter_commandscript : public CommandScript
{
public:
    deserter_commandscript() : CommandScript("deserter_commandscript") { }

    /**
    * @brief Returns the command structure for the system.
    */

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable deserterInstanceCommandTable =
        {
            { "add",        HandleDeserterInstanceAdd,       SEC_ADMINISTRATOR, Console::Yes },
            { "remove all", HandleDeserterInstanceRemoveAll, SEC_ADMINISTRATOR, Console::Yes },
            { "remove",     HandleDeserterInstanceRemove,    SEC_ADMINISTRATOR, Console::Yes }
        };
        static ChatCommandTable deserterBGCommandTable =
        {
            { "add",        HandleDeserterBGAdd,       SEC_ADMINISTRATOR, Console::Yes },
            { "remove all", HandleDeserterBGRemoveAll, SEC_ADMINISTRATOR, Console::Yes },
            { "remove",     HandleDeserterBGRemove,    SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable deserterCommandTable =
        {
            { "instance", deserterInstanceCommandTable },
            { "bg",       deserterBGCommandTable }
        };
        static ChatCommandTable commandTable =
        {
            { "deserter", deserterCommandTable }
        };
        return commandTable;
    }

    /**
    * @brief Applies the Deserter Debuff to a player
    *
    * This function applies a Deserter Debuff of the given type (Instance or BG) to the
    * selected player, with the provided duration in seconds.
    *
    * @param handler The ChatHandler, passed by the system.
    * @param time The provided duration in seconds.
    * @param isInstance provided by the relaying functions, so we don't have
    * to write that much code :)
    *
    * @return true if everything was correct, false if an error occured.
    *
    * Example Usage:
    * @code
    * .deserter instance add 3600 (one hour)
    * -or-
    * .deserter bg add 3600 (one hour)
    * @endcode
    */
    static bool HandleDeserterAdd(ChatHandler* handler, Optional<PlayerIdentifier> player, uint32 time, bool isInstance)
    {
        if (!player)
        {
            player = PlayerIdentifier::FromTargetOrSelf(handler);
        }

        if (!player)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!time)
        {
            handler->SendSysMessage(LANG_BAD_VALUE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* target = player->GetConnectedPlayer();

        if (target)
        {
            Aura* aura = target->AddAura(isInstance ? LFG_SPELL_DUNGEON_DESERTER : BG_SPELL_DESERTER, target);

            if (!aura)
            {
                handler->SendSysMessage(LANG_BAD_VALUE);
                handler->SetSentErrorMessage(true);
                return false;
            }
            aura->SetDuration(time * IN_MILLISECONDS);

            return true;
        }

        uint8 index = 0;
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_AURA);
        stmt->SetData(index++, player->GetGUID().GetCounter());
        stmt->SetData(index++, player->GetGUID().GetCounter());
        stmt->SetData(index++, 0);
        stmt->SetData(index++, isInstance ? LFG_SPELL_DUNGEON_DESERTER : BG_SPELL_DESERTER);
        stmt->SetData(index++, 1);
        stmt->SetData(index++, 1);
        stmt->SetData(index++, 1);
        stmt->SetData(index++, 0);
        stmt->SetData(index++, 0);
        stmt->SetData(index++, 0);
        stmt->SetData(index++, 0);
        stmt->SetData(index++, 0);
        stmt->SetData(index++, 0);
        stmt->SetData(index++, isInstance ? 1800000 : 900000);
        stmt->SetData(index++, time * 1000);
        stmt->SetData(index, 0);
        CharacterDatabase.Execute(stmt);

        return true;
    }

    /**
    * @brief Removes the Deserter Debuff from a player
    *
    * This function removes a Deserter Debuff of the given type (Instance or BG) from the
    * selected player.
    *
    * @param handler The ChatHandler, passed by the system.
    * @param isInstance provided by the relaying functions, so we don't have
    * to write that much code :)
    *
    * @return true if everything was correct, false if an error occured.
    *
    * Example Usage:
    * @code
    * .deserter instance remove
    * -or-
    * .deserter bg remove
    * @endcode
    */
    static bool HandleDeserterRemove(ChatHandler* handler, Optional<PlayerIdentifier> player, bool isInstance)
    {
        if (!player)
        {
            player = PlayerIdentifier::FromTargetOrSelf(handler);
        }

        if (!player)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* target = player->GetConnectedPlayer();

        if (target)
        {
            target->RemoveAura(isInstance ? LFG_SPELL_DUNGEON_DESERTER : BG_SPELL_DESERTER);
            return true;
        }

        CharacterDatabase.Query("DELETE FROM character_aura WHERE guid = {} AND spell = {}", player->GetGUID().GetCounter(), isInstance ? LFG_SPELL_DUNGEON_DESERTER : BG_SPELL_DESERTER);

        return true;
    }

    static bool HandleDeserterRemoveAll(ChatHandler* handler, bool isInstance)
    {
        CharacterDatabase.Query("DELETE FROM character_aura WHERE spell = {} AND remainTime <= 1800000", isInstance ? LFG_SPELL_DUNGEON_DESERTER : BG_SPELL_DESERTER);

        std::shared_lock<std::shared_mutex> lock(*HashMapHolder<Player>::GetLock());
        HashMapHolder<Player>::MapType const& onlinePlayerList = ObjectAccessor::GetPlayers();
        for (HashMapHolder<Player>::MapType::const_iterator itr = onlinePlayerList.begin(); itr != onlinePlayerList.end(); ++itr)
        {
            Player* player = itr->second;
            Aura* aura = player->GetAura(isInstance ? LFG_SPELL_DUNGEON_DESERTER : BG_SPELL_DESERTER);
            if (aura && aura->GetDuration() <= 1800000)
            {
                player->RemoveAura(isInstance ? LFG_SPELL_DUNGEON_DESERTER : BG_SPELL_DESERTER);
            }
        }

        handler->PSendSysMessage("%s Deserter has been removed from all players", isInstance ? "Instance" : "Battleground");
        return true;
    }

    /// @sa HandleDeserterAdd()
    static bool HandleDeserterInstanceAdd(ChatHandler* handler, Optional<PlayerIdentifier> player, uint32 time)
    {
        return HandleDeserterAdd(handler, player, time, true);
    }

    /// @sa HandleDeserterAdd()
    static bool HandleDeserterBGAdd(ChatHandler* handler, Optional<PlayerIdentifier> player, uint32 time)
    {
        return HandleDeserterAdd(handler, player, time, false);
    }

    /// @sa HandleDeserterRemove()
    static bool HandleDeserterInstanceRemove(ChatHandler* handler, Optional<PlayerIdentifier> player)
    {
        return HandleDeserterRemove(handler, player, true);
    }

    /// @sa HandleDeserterRemove()
    static bool HandleDeserterBGRemove(ChatHandler* handler, Optional<PlayerIdentifier> player)
    {
        return HandleDeserterRemove(handler, player, false);
    }

    static bool HandleDeserterInstanceRemoveAll(ChatHandler* handler)
    {
        return HandleDeserterRemoveAll(handler, true);
    }

    static bool HandleDeserterBGRemoveAll(ChatHandler* handler)
    {
        return HandleDeserterRemoveAll(handler, false);
    }
};

void AddSC_deserter_commandscript()
{
    new deserter_commandscript();
}
