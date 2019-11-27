/**
 * @author      : theanswer (theanswer@Victors-MacBook-Pro.local)
 * @file        : RESTServer
 * @created     : Sunday Nov 11, 2018 11:31:08 CET
 */

#include "RESTServer.h"

using namespace server::web3;

const Logger server::web3::RESTServer::logger("RESTServer");

RESTServer::RESTServer(uint16 port) : doRun(true), port(port) {

}

RESTServer::~RESTServer() {
}

#ifdef COMPILE_CORE3_REST
#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include <map>
#include <set>
#include <string>
#include <memory>
#include <chrono>

#include "engine/engine.h"
#include "conf/ConfigManager.h"

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

using namespace std;

namespace server {
namespace web3 {
	// simple API Authtoken check
	bool check_auth(http_request request) {
		auto apiAuthToken = ConfigManager::instance()->getString("Core3.RESTServer.APIToken", "");

		if (apiAuthToken.length() == 0) {
			RESTServer::logger.error("Core3.RESTServer.APIToken not set, refusing to authorize API call.");
			return false;
		}

		if (apiAuthToken.length() < 15) {
			RESTServer::logger.error("Core3.RESTServer.APIToken too short, must be at least 15 characters, refusing to authorize API call.");
			return false;
		}

		auto headers = request.headers();

		if (request.headers().find(header_names::authorization) == headers.end())
			return false;

		auto requestToken = String(headers[header_names::authorization].c_str());

		if (requestToken == apiAuthToken)
			return true;

		return false;
	}

	void handle_get(http_request request) {
		auto start = chrono::steady_clock::now();

		if (!check_auth(request)) {
			request.reply(status_codes::Forbidden, U("Invalid API Token"));
			return;
		}

		const auto& uri = request.relative_uri();

		auto fragments = uri::split_path(uri.to_string());

		nlohmann::json responses = nlohmann::json::array();

		for (const auto& frag : fragments) {
			try {
				uint64 oid = UnsignedLong::valueOf(frag.c_str());

				auto obj = Core::lookupObject(oid).castTo<ManagedObject*>();

				if (obj != nullptr) {
					ReadLocker lock(obj);

					nlohmann::json jsonData;
					obj->writeJSON(jsonData);

					nlohmann::json entry;
					entry[frag] = jsonData;

					responses.push_back(entry);
				}
			} catch (const Exception& e) {
				RESTServer::logger.error(e.getMessage());
			}
		}

		http_response response(status_codes::Accepted);
		response.headers()[U("Access-Control-Allow-Origin")] = U("*");

		if (responses.empty()) {
			response.set_status_code(status_codes::NotFound);
		} else {
			response.set_status_code(status_codes::OK);

			response.set_body(json::value::parse(responses.dump()));
		}

		auto end = chrono::steady_clock::now();
		auto diff = end - start;

		response.headers()[U("ms")] = String::valueOf(chrono::duration <double, milli> (diff).count()).toCharArray();

		request.reply(response);
	}

	static UniqueReference<http_listener*> restListener;
}
}

void RESTServer::start() {
	if (restListener != nullptr) {
		restListener->close().wait();
	}

	restListener = new http_listener(("http://0.0.0.0:" + String::valueOf(port) + "/object").toCharArray());

	restListener->support(methods::GET, handle_get);
	//listener.support(methods::POST, handle_post);
	//listener.support(methods::PUT, handle_put);
	//listener.support(methods::DEL, handle_del);

	try {
		restListener->open()
			.then([this] {
			logger.info("listening to port " + String::valueOf(port), true);
		}).wait();
	} catch (exception const & e) {
		logger.error(e.what());
	}
}


void RESTServer::stop() {
	doRun.set(false);

	if (restListener != nullptr) {
		restListener->close().wait();
		restListener = nullptr;
	}
}

#else

void RESTServer::start() {
	logger.error("disabled, server compiled without rest support");
}

void RESTServer::stop() {
	doRun.set(false);
}

#endif

