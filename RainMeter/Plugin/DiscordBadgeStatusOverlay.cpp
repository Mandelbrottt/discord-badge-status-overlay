#define NOMINMAX
#include <Windows.h>

#include "rainmeter-plugin-sdk/API/RainmeterAPI.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

struct Measure
{
	void* rm = nullptr;

	std::wstring badgeStatus;

	std::wstring betterDiscordConfigPath;
	std::wstring discordExe;
};

void MarkBadgeStatusFailed(Measure* measure)
{
	measure->badgeStatus = L"fail";
	RmLogF(measure->rm, LOG_ERROR, L"Failed to read badgeStatus. Do you maybe need to update BetterDiscord?");
}

PLUGIN_EXPORT void Initialize(void** data, void* rm)
{
	Measure* measure = new Measure;
	measure->rm = rm;
	*data = measure;

	RmLog(rm, LOG_NOTICE, L"Discord Badge Overlay has started!");
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue)
{
	Measure* measure = (Measure*) data;
	measure->betterDiscordConfigPath = RmReadPath(rm, L"DiscordPluginConfigPath", nullptr);
	
	if (measure->betterDiscordConfigPath.empty())
	{
		RmLog(rm, LOG_ERROR, L"Failed to read DiscordPluginConfigPath variable from config!");
		MarkBadgeStatusFailed(measure);
		return;
	}
	RmLogF(rm, LOG_NOTICE, L"Found DiscordPluginConfigPath as \"%s\"", measure->betterDiscordConfigPath.c_str());
	
	fs::path discordPath = RmReadPath(rm, L"DiscordInstallDir", nullptr);
	if (discordPath.empty())
	{
		RmLog(rm, LOG_ERROR, L"Failed to read DiscordInstallDir variable from config!");
		MarkBadgeStatusFailed(measure);
		return;
	}
	RmLogF(rm, LOG_DEBUG, L"Found DiscordInstallDir as \"%ws\"", discordPath.c_str());

	std::wstring pathName;
	for (const auto& entry : fs::directory_iterator(discordPath))
	{
		auto fileName = entry.path().filename();
		if (fileName.string().rfind("app-", 0) != 0)
		{
			continue;
		}

		if (pathName.empty())
		{
			pathName = fileName;
			continue;
		}

		if (pathName < fileName)
		{
			pathName = fileName;
		}
	}

	if (pathName.empty())
	{
		RmLog(rm, LOG_ERROR, L"Failed to find a valid Discord.exe!");
		MarkBadgeStatusFailed(measure);
		return;
	}

	measure->discordExe = absolute(discordPath / pathName / "Discord.exe");
	RmLogF(rm, LOG_DEBUG, L"Found Discord Exe \"%s\"", measure->discordExe.c_str());
	
	measure->badgeStatus = std::to_wstring(0);
}

PLUGIN_EXPORT double Update(void* data)
{
	Measure* measure = (Measure*) data;

	auto is = std::ifstream(measure->betterDiscordConfigPath);
	std::stringstream buffer;
	std::string fileContents;

	if (!is)
	{
		MarkBadgeStatusFailed(measure);
		return 0;
	}
	
	buffer << is.rdbuf();
	is.close();

	fileContents = buffer.str();
	size_t index = fileContents.find("\"badgeStatus\":");
	const char* buf = fileContents.c_str() + index;

	if (index == std::string::npos)
	{
		MarkBadgeStatusFailed(measure);
		return 0;
	}

	int result = 0;

	if (sscanf_s(buf, "\"badgeStatus\": %d", &result) == 0)
	{
		MarkBadgeStatusFailed(measure);
		return 0;
	}

	// Cap the result to 10, and change -1 to 11 to follow the naming convention of the icons
	// 0 = no notification, 1-10 = 1-9+ mentions, 11 = non-mention message
	result = std::min(result, 10);
	if (result == -1)
	{
		result = 11;
	}

	std::wstring badgeStatus = std::to_wstring(result);
	if (badgeStatus == measure->badgeStatus)
	{
		return 0;
	}
	
	measure->badgeStatus = std::to_wstring(result);
	RmLogF(measure->rm, LOG_DEBUG, L"Setting badgeStatus to %s", measure->badgeStatus.c_str());
	
	return 0;
}

PLUGIN_EXPORT LPCWSTR GetAppExe(void* data, const int argc, const WCHAR* argv[])
{
	Measure* measure = (Measure*) data;
	return measure->discordExe.c_str();
}

PLUGIN_EXPORT LPCWSTR GetBadgeStatus(void* data, const int argc, const WCHAR* argv[])
{
	Measure* measure = (Measure*) data;
	return measure->badgeStatus.c_str();
}

PLUGIN_EXPORT void Finalize(void* data)
{
	Measure* measure = (Measure*) data;
	delete measure;
}
