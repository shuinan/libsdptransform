#include "sdptransform.hpp"
#include <stdexcept>
#include <cstddef> // size_t
#include <memory>  // std::addressof()
#include <sstream> // std::stringstream

using json = nlohmann::json;

namespace sdptransform
{
	void parseReg(const grammar::Rule& rule, json& location, const std::string& content);

	void attachProperties(
		const std::smatch& match,
		json& location,
		const std::vector<std::string>& names,
		const std::string& rawName
	);

	json toIntIfInt(const std::ssub_match& subMatch);

	bool isNumber(const std::string& string);

	void parse(const std::string& sdp, json& session)
	{
		static const std::regex ValidLine("^([a-z])=(.*)");

		if (!session.is_object())
			throw std::invalid_argument("given session is not a JSON object");

		std::stringstream sdpstream(sdp);
		std::string line;
		json media = json::array();
		json* location = std::addressof(session);

		while (std::getline(sdpstream, line, '\n'))
		{
			// Remove \r if lines are separated with \r\n (as mandated in SDP).
			if (line[line.length() - 1] == '\r')
				line.pop_back();

			// Ensure it's a valid SDP line.
			if (!std::regex_match(line, ValidLine))
				continue;

			char type = line[0];
			std::string content = line.substr(2);

			if (type == 'm')
			{
				json m = json::object();

				m["rtp"] = json::array();
				m["fmtp"] = json::array();

				media.push_back(m);
				location = std::addressof(media[media.size() - 1]); // Point at latest media line.
			}

			if (grammar::mapRules.find(type) == grammar::mapRules.end())
				continue;

			for (int j = 0; j < grammar::mapRules.at(type).size(); ++j)
			{
				auto& rule = grammar::mapRules.at(type)[j];

				if (std::regex_match(content, rule.reg))
				{
					parseReg(rule, *location, content);

					break;
				}
			}
		}

		session["media"] = media; // Link it up.
	}

	void parseReg(const grammar::Rule& rule, json& location, const std::string& content)
	{
		bool needsBlank = !rule.name.empty() && !rule.names.empty();

		if (!rule.push.empty() && location.find(rule.push) == location.end())
		{
			location[rule.push] = json::array();
		}
		else if (needsBlank && location.find(rule.name) == location.end())
		{
			location[rule.name] = json::object();
		}

		std::smatch match;

		std::regex_match(content, match, rule.reg);

		json object = json::object();
		json& keyLocation = !rule.push.empty() ?
			object : // Blank object that will be pushed.
			needsBlank ? location[rule.name] : location; // Otherwise named location or root.

		attachProperties(match, keyLocation, rule.names, rule.name);

		if (!rule.push.empty())
			location[rule.push].push_back(keyLocation);
	}

	void attachProperties(
		const std::smatch& match,
		json& location,
		const std::vector<std::string>& names,
		const std::string& rawName
	)
	{
		if (!rawName.empty() && names.empty())
		{
			location[rawName] = toIntIfInt(match[1]);
		}
		else
		{
			for (size_t i = 0; i < names.size(); ++i)
			{
				if (i + 1 < match.size())
					location[names[i]] = toIntIfInt(match[i + 1]);
			}
		}
	}

	json toIntIfInt(const std::ssub_match& subMatch)
	{
		auto string = subMatch.str();

		if (!isNumber(string))
			return string;

		try
		{
			return std::stoll(string);
		}
		catch (const std::exception& error)
		{
			return string;
		}
	}

	bool isNumber(const std::string& string)
	{
		return (
			!string.empty() &&
			std::find_if(
				string.begin(),
				string.end(),
				[](char c) { return !std::isdigit(c); }) == string.end()
		);
	}
}