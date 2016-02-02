/*******************************************************************************
 * Copyright 2014, 2015 IBM Corp.
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
 *******************************************************************************/

#include "AgentExtensions.h"
#include "Typesdef.h"
#include "uv.h"
#include <cstring>
#include <string>
#include <sstream>

#define DEFAULT_CAPACITY 10240

#if defined(_WINDOWS)
#define EVENTLOOPPLUGIN_DECL __declspec(dllexport)	/* required for DLLs to export the plugin functions */
#else
#define EVENTLOOPPLUGIN_DECL 
#endif

namespace plugin {
	agentCoreFunctions api;
	uint32 provid = 0;	
	uv_prepare_t *prepareHandle;
	uv_check_t *checkHandle;
	bool firstPrepare;
	bool firstCheck;
	uint64 previousTimeAtLoopStartInMSCheck;
	uint64 previousTimeAtLoopStartInMSPrepare;
	uint64 previousTimeAtCheckInNS;
	uint64 previousTimeAtPrepareInNS;
	//uv_idle_t *idleHandle;
}

using namespace ibmras::common::logging;

void OnEventLoopIterationCheck(uv_check_t *handle) {
	uint64 timeAtLoopStartInMS = uv_now(uv_default_loop());
	uint64 timeAtCheckInNS = uv_hrtime();
	std::stringstream ss;
	ss << "NodeEventLoop,Check__";
	ss << "," << timeAtLoopStartInMS;
	ss << "," << timeAtCheckInNS;
	if (plugin::firstCheck) {
		plugin::firstCheck = false;
		ss << ",0,0";
	} else {
		ss << "," << (timeAtLoopStartInMS - plugin::previousTimeAtLoopStartInMSCheck);
		ss << "," << (timeAtCheckInNS - plugin::previousTimeAtCheckInNS);
	}
	ss << std::endl;
	std::string payload = ss.str();

	plugin::previousTimeAtLoopStartInMSCheck = timeAtLoopStartInMS;
	plugin::previousTimeAtCheckInNS = timeAtCheckInNS;

	monitordata data;
	data.persistent = false;
	data.provID = plugin::provid;
	data.sourceID = 0;
	data.size = static_cast<uint32>(payload.length());
	data.data = payload.c_str();
	plugin::api.agentPushData(&data);
}

void OnEventLoopIterationPrepare(uv_prepare_t *handle) {
//void OnEventLoopIteration(uv_idle_t *handle) {
	uint64 timeAtLoopStartInMS = uv_now(uv_default_loop());
	uint64 timeAtPrepareInNS = uv_hrtime();
	std::stringstream ss;
	ss << "NodeEventLoop,Prepare";
	ss << "," << timeAtLoopStartInMS;
	ss << "," << timeAtPrepareInNS;
	if (plugin::firstPrepare) {
		plugin::firstPrepare = false;
		ss << ",0,0";
	} else {
		ss << "," << (timeAtLoopStartInMS - plugin::previousTimeAtLoopStartInMSPrepare);
		ss << "," << (timeAtPrepareInNS - plugin::previousTimeAtPrepareInNS);
	}
	ss << std::endl;
	std::string payload = ss.str();

	plugin::previousTimeAtLoopStartInMSPrepare = timeAtLoopStartInMS;
	plugin::previousTimeAtPrepareInNS = timeAtPrepareInNS;

	monitordata data;
	data.persistent = false;
	data.provID = plugin::provid;
	data.sourceID = 0;
	data.size = static_cast<uint32>(payload.length());
	data.data = payload.c_str();
	plugin::api.agentPushData(&data);
}

static void cleanupHandle(uv_handle_t *handle) {
	delete handle;
}

static char* NewCString(const std::string& s) {
	char *result = new char[s.length() + 1];
	std::strcpy(result, s.c_str());
	return result;
}

pushsource* createPushSource(uint32 srcid, const char* name) {
	pushsource *src = new pushsource();
	src->header.name = name;
	std::string desc("Description for ");
	desc.append(name);
	src->header.description = NewCString(desc);
	src->header.sourceID = srcid;
	src->next = NULL;
	src->header.capacity = DEFAULT_CAPACITY;
	return src;
}

extern "C" {
	EVENTLOOPPLUGIN_DECL pushsource* ibmras_monitoring_registerPushSource(agentCoreFunctions api, uint32 provID) {
		plugin::api = api;
	
		plugin::api.logMessage(debug, "[eventloop_node] Registering push sources");
		pushsource *head = createPushSource(0, "eventloop_node");
		plugin::provid = provID;
		return head;
	}
	
	EVENTLOOPPLUGIN_DECL int ibmras_monitoring_plugin_init(const char* properties) {
		// NOTE(tunniclm): We don't have the agentCoreFunctions yet, so we can't do any init that requires
		//                 calling into the API (eg getting properties.)	
		return 0;
	}
	
	EVENTLOOPPLUGIN_DECL int ibmras_monitoring_plugin_start() {
		plugin::api.logMessage(debug, "[eventloop_node] Starting");
		plugin::firstPrepare = true;
		plugin::firstCheck = true;
		plugin::prepareHandle = new uv_prepare_t();
		plugin::checkHandle = new uv_check_t();
		uv_prepare_init(uv_default_loop(), plugin::prepareHandle);
		uv_prepare_start(plugin::prepareHandle, OnEventLoopIterationPrepare);
		uv_check_init(uv_default_loop(), plugin::checkHandle);
		uv_check_start(plugin::checkHandle, OnEventLoopIterationCheck);
		//plugin::idleHandle = new uv_idle_t();
		//uv_idle_init(uv_default_loop(), plugin::idleHandle);
		//uv_idle_start(plugin::idleHandle, OnEventLoopIteration);
	
		return 0;
	}
	
	EVENTLOOPPLUGIN_DECL int ibmras_monitoring_plugin_stop() {
		plugin::api.logMessage(fine, "[eventloop_node] Stopping");
	
		uv_prepare_stop(plugin::prepareHandle);
		uv_close((uv_handle_t*) plugin::prepareHandle, cleanupHandle);
		//uv_idle_stop(plugin::idleHandle);
		//uv_close((uv_handle_t*) plugin::idleHandle, cleanupHandle);
	
		return 0;
	}
	
	EVENTLOOPPLUGIN_DECL const char* ibmras_monitoring_getVersion() {
		return "1.0";
	}
}
