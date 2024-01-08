////////////////////////////////////////////////////////////////////////
//
// This file is part of sc4-city-date-sync, a DLL Plugin for SimCity 4
// that synchronizes the city dates in a region.
//
// Copyright (c) 2023, 2024 Nicholas Hayes
//
// This file is licensed under terms of the MIT License.
// See LICENSE.txt for more information.
//
////////////////////////////////////////////////////////////////////////

#include "version.h"
#include "cIGZFrameWork.h"
#include "cIGZApp.h"
#include "cIGZCheatCodeManager.h"
#include "cIGZDate.h"
#include "cISC4App.h"
#include "cISC4City.h"
#include "cISC4Simulator.h"
#include "cIGZMessageServer2.h"
#include "cIGZMessageTarget.h"
#include "cIGZMessageTarget2.h"
#include "cIGZString.h"
#include "cRZMessage2COMDirector.h"
#include "cRZMessage2Standard.h"
#include "cRZBaseString.h"
#include "GZServPtrs.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <Windows.h>
#include "wil/resource.h"
#include "wil/win32_helpers.h"

static constexpr uint32_t kSC4MessagePostCityInit = 0x26D31EC1;
static constexpr uint32_t kSC4MessagePreCityShutdown = 0x26D31EC2;
static constexpr uint32_t kSC4MessagePostSave = 0x26C63345;
static constexpr uint32_t kSC4MessagePostRegionInit = 0xCBB5BB45;

static constexpr uint32_t kCityDateSyncPluginDirectorID = 0x9f234be4;

static const uint32_t kGZIID_cISC4App = 0x26ce01c0;
static constexpr uint32_t kSC4CheatGUIDSimDate = 0x8a78beff;

static constexpr std::string_view PluginLogFileName = "SC4CityDateSync.log";

class cGZCityDateSyncDllDirector : public cRZMessage2COMDirector
{
public:

	cGZCityDateSyncDllDirector()
		: currentSimDate(nullptr),
		  exitingCity(false)
	{
		std::filesystem::path dllFolder = GetDllFolderPath();

		std::filesystem::path logFilePath = dllFolder;
		logFilePath /= PluginLogFileName;

		logFile.open(logFilePath, std::ofstream::out | std::ofstream::trunc);
		if (logFile)
		{
			logFile << "SC4CityDateSync v" PLUGIN_VERSION_STR << std::endl;
		}
		cLocale = _create_locale(LC_ALL, "C");
	}

	uint32_t GetDirectorID() const
	{
		return kCityDateSyncPluginDirectorID;
	}

	void WriteLogEntryFormattedInvariant(const char* format, ...)
	{
		va_list args;
		va_start(args, format);

		va_list argsCopy;
		va_copy(argsCopy, args);

		int formattedStringLength = _vscprintf_l(format, cLocale, argsCopy);

		va_end(argsCopy);

		if (formattedStringLength > 0)
		{
			size_t formattedStringLengthWithNull = static_cast<size_t>(formattedStringLength) + 1;

			std::unique_ptr<char[]> buffer = std::make_unique_for_overwrite<char[]>(formattedStringLengthWithNull);

			_vsnprintf_s_l(buffer.get(), formattedStringLengthWithNull, _TRUNCATE, format, cLocale, args);

			WriteLogEntry(buffer.get());
		}

		va_end(args);
	}

	void WriteLogEntry(const char* line)
	{
		if (logFile && line)
		{
			char timeBuffer[256]{};
			GetTimeFormatA(
				LOCALE_USER_DEFAULT,
				0,
				nullptr,
				nullptr,
				timeBuffer,
				_countof(timeBuffer));

			int formattedStringLength = std::snprintf(
				nullptr,
				0,
				"%s %s",
				timeBuffer,
				line);

			if (formattedStringLength > 0)
			{
				size_t formattedStringLengthWithNull = static_cast<size_t>(formattedStringLength) + 1;

				std::unique_ptr<char[]> buffer = std::make_unique_for_overwrite<char[]>(formattedStringLengthWithNull);

				std::snprintf(
					buffer.get(),
					formattedStringLengthWithNull,
					"%s %s",
					timeBuffer,
					line);

#ifdef _DEBUG
				PrintLineToDebugOutput(buffer.get());
#endif // _DEBUG

				logFile << buffer.get() << std::endl;
			}
		}
	}

	void PostCityInit(cIGZMessage2Standard* pStandardMsg)
	{
		cISC4City* pCity = reinterpret_cast<cISC4City*>(pStandardMsg->GetIGZUnknown());

		if (pCity)
		{
			if (pCity->GetEstablished())
			{
				cISC4Simulator* pSimulator = pCity->GetSimulator();

				if (pSimulator)
				{
					if (currentSimDate)
					{
						cIGZDate* cityDate = pSimulator->GetSimDate();

						if (cityDate)
						{
							if (*cityDate < *currentSimDate)
							{
								cISC4AppPtr pSC4App;

								if (pSC4App)
								{
									cIGZCheatCodeManager* pCheatCodeManager = pSC4App->GetCheatCodeManager();

									if (pCheatCodeManager)
									{
										cIGZDate* previousDay = nullptr;
										if (currentSimDate->Clone(&previousDay))
										{
											// SimDate MM DD YYYY sets the calendar date to the next day after the date
											// you enter, so you have to subtract one day from your selected value when
											// using it.
											// For example, to set the simulator date to 1/1/2000 you would have to enter the
											// command: SimDate 12 31 1999.
											*previousDay -= 1;

											char simDateCommandBuffer[1024]{};

											// We print the number using the "C" locale, this ensures that the number will
											// always be printed the same way regardless of the global locale settings that
											// are in use.
											int charsWritten = _snprintf_s_l(
												simDateCommandBuffer,
												sizeof(simDateCommandBuffer),
												_TRUNCATE,
												"SimDate %u %u %4u",
												cLocale,
												previousDay->Month(),
												previousDay->DayOfMonth(),
												previousDay->Year());

											if (charsWritten > 0)
											{
												// Cache the old city date in local variables for logging purposes.
												// Because the cityDate variable is a pointer to an internal game structure its
												// date value will always be the current simulator date at the time its methods
												// are called, and this isn't what we want.
												uint32_t oldCityMonth = cityDate->Month();
												uint32_t oldCityDayOfMonth = cityDate->DayOfMonth();
												uint32_t oldCityYear = cityDate->Year();

												// SendCheatNotifications always returns false.
												pCheatCodeManager->SendCheatNotifications(
													cRZBaseString(simDateCommandBuffer),
													kSC4CheatGUIDSimDate);

												WriteLogEntryFormattedInvariant(
													"Changed the city date from %u %u %4u to %u %u %4u.",
													oldCityMonth,
													oldCityDayOfMonth,
													oldCityYear,
													currentSimDate->Month(),
													currentSimDate->DayOfMonth(),
													currentSimDate->Year());
											}
											else
											{
												WriteLogEntry("Failed to create the SimDate cheat command.");
											}
										}
									}
								}
							}
							else
							{
								WriteLogEntry("The city has a more recent date than the previous city.");
							}
						}
						else
						{
							WriteLogEntry("Unable to check the date because the date pointer was null.");
						}
						currentSimDate = nullptr;
					}
					else
					{
						WriteLogEntry("The city date has not been set for the current region.");
					}
				}
				else
				{
					WriteLogEntry("Unable to check the date because the simulator pointer was null.");
				}
			}
			else
			{
				WriteLogEntry("The city has not been established, once it is established the date may be changed the next time it is loaded.");
			}
		}
		else
		{
			WriteLogEntry("Unable to check the date because the city pointer was null.");
		}
	}

	void PostCitySave()
	{
		cISC4AppPtr pSC4App;

		if (pSC4App)
		{
			cISC4City* pCity = pSC4App->GetCity();

			if (pCity)
			{
				if (pCity->GetEstablished())
				{
					cISC4Simulator* pSimulator = pCity->GetSimulator();

					if (pSimulator)
					{
						cIGZDate* simDate = pSimulator->GetSimDate();

						if (simDate)
						{
							if (simDate->Clone(&currentSimDate))
							{
								WriteLogEntryFormattedInvariant(
									"Saved the current city date: %u %u %4u.",
									currentSimDate->Month(),
									currentSimDate->DayOfMonth(),
									currentSimDate->Year());
							}
							else
							{
								WriteLogEntry("Failed to copy the current city date.");
							}
						}
						else
						{
							WriteLogEntry("Ignoring the date because the city date pointer was null.");
						}
					}
					else
					{
						WriteLogEntry("Ignoring the date because the simulator pointer was null.");
					}
				}
				else
				{
					WriteLogEntry("Ignoring the date because the city has not been established.");
				}
			}
			else
			{
				WriteLogEntry("Ignoring the date because the city pointer was null.");
			}
		}
		else
		{
			WriteLogEntry("Ignoring the date because the cISC4App pointer was null.");
		}
	}

	void PreCityShutdown(cIGZMessage2Standard* pStandardMsg)
	{
		exitingCity = true;
	}

	void PostRegionInit()
	{
		// We ignore the first PostRegionInit message after exiting a city.
		// If a second PostRegionInit message is received it indicates that
		// the player has changed regions.
		if (exitingCity)
		{
			exitingCity = false;
			return;
		}

		// The city dates are not synchronized across regions.
		currentSimDate = nullptr;
	}

	bool DoMessage(cIGZMessage2* pMessage)
	{
		cIGZMessage2Standard* pStandardMsg = static_cast<cIGZMessage2Standard*>(pMessage);
		uint32_t dwType = pMessage->GetType();

		switch (dwType)
		{
		case kSC4MessagePostCityInit:
			PostCityInit(pStandardMsg);
			break;
		case kSC4MessagePreCityShutdown:
			PreCityShutdown(pStandardMsg);
			break;
		case kSC4MessagePostSave:
			PostCitySave();
			break;
		case kSC4MessagePostRegionInit:
			PostRegionInit();
			break;
		}

		return true;
	}

	bool PostAppInit()
	{
		cIGZFrameWork* const pFramework = RZGetFrameWork();
		if (!pFramework)
		{
			WriteLogEntry("Failed to get the GZCOM framework pointer.");
			return true;
		}

		cIGZApp* const pApp = pFramework->Application();
		if (!pFramework)
		{
			WriteLogEntry("Failed to get the GZCOM Application pointer.");
			return true;
		}

		cISC4App* pSC4App = nullptr;
		if (!pApp->QueryInterface(kGZIID_cISC4App, reinterpret_cast<void**>(&pSC4App)))
		{
			WriteLogEntry("Failed to get the SC4 Application pointer.");
			return true;
		}

		// The SimDate command requires the game's debug mode to be enabled.
		pSC4App->SetDebugFunctionalityEnabled(true);

		cIGZMessageServer2Ptr pMsgServ;
		if (pMsgServ)
		{
			std::vector<uint32_t> requiredNotifications;
			requiredNotifications.push_back(kSC4MessagePostCityInit);
			requiredNotifications.push_back(kSC4MessagePreCityShutdown);
			requiredNotifications.push_back(kSC4MessagePostSave);
			requiredNotifications.push_back(kSC4MessagePostRegionInit);

			for (uint32_t messageID : requiredNotifications)
			{
				if (!pMsgServ->AddNotification(this, messageID))
				{
					WriteLogEntry("Failed to subscribe to the required notifications.");
					return true;
				}
			}
		}
		else
		{
			WriteLogEntry("Failed to subscribe to the required notifications.");
		}
		return true;
	}

	bool OnStart(cIGZCOM* pCOM)
	{
		cIGZFrameWork* const pFramework = RZGetFrameWork();

		if (pFramework->GetState() < cIGZFrameWork::kStatePreAppInit)
		{
			pFramework->AddHook(this);
		}
		else
		{
			PreAppInit();
		}
		return true;
	}

private:

	std::filesystem::path GetDllFolderPath()
	{
		wil::unique_cotaskmem_string modulePath = wil::GetModuleFileNameW(wil::GetModuleInstanceHandle());

		std::filesystem::path temp(modulePath.get());

		return temp.parent_path();
	}

#ifdef _DEBUG
	void PrintLineToDebugOutputFormatted(const char* format, ...)
	{
		char buffer[1024]{};

		va_list args;
		va_start(args, format);

		std::vsnprintf(buffer, sizeof(buffer), format, args);

		va_end(args);

		PrintLineToDebugOutput(buffer);
	}

	void PrintLineToDebugOutput(const char* line)
	{
		OutputDebugStringA(line);
		OutputDebugStringA("\n");
	}
#endif // _DEBUG

	cIGZDate* currentSimDate;
	bool exitingCity;
	_locale_t cLocale;
	std::ofstream logFile;
};

cRZCOMDllDirector* RZGetCOMDllDirector() {
	static cGZCityDateSyncDllDirector sDirector;
	return &sDirector;
}