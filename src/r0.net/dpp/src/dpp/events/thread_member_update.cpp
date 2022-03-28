/************************************************************************************
 *
 * D++, A Lightweight C++ library for Discord
 *
 * Copyright 2021 Craig Edwards and D++ contributors 
 * (https://github.com/brainboxdotcc/DPP/graphs/contributors)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/
#include "dpp_pch.h"
#include <dpp/discord.h>
#include <dpp/event.h>
#include <string>
#include <iostream>
#include <fstream>
#include <dpp/discordclient.h>
#include <dpp/discordevents.h>
#include <dpp/discord.h>
#include <dpp/cache.h>
#include <dpp/stringops.h>
#include "nlohmann/json.hpp"
#include "fmt/format.h"

using json = nlohmann::json;

namespace dpp { namespace events {

using namespace dpp;
void thread_member_update::handle(discord_client* client, json& j, const std::string& raw) {
	if (!client->creator->on_thread_member_update.empty()) {
		json& d = j["d"];
		dpp::thread_member_update_t tm(client, raw);
		tm.updated = thread_member().fill_from_json(&d);
		client->creator->on_thread_member_update.call(tm);
	}
}
}};