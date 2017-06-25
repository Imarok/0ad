/* Copyright (C) 2017 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "Replay.h"

#include "graphics/TerrainTextureManager.h"
#include "lib/timer.h"
#include "lib/file/file_system.h"
#include "lib/res/h_mgr.h"
#include "lib/tex/tex.h"
#include "ps/Game.h"
#include "ps/CLogger.h"
#include "ps/Loader.h"
#include "ps/Mod.h"
#include "ps/Profile.h"
#include "ps/ProfileViewer.h"
#include "ps/Pyrogenesis.h"
#include "ps/Util.h"
#include "ps/VisualReplay.h"
#include "scriptinterface/ScriptInterface.h"
#include "scriptinterface/ScriptStats.h"
#include "simulation2/Simulation2.h"
#include "simulation2/helpers/SimulationCommand.h"

#include <ctime>
#include <fstream>

CReplayLogger::CReplayLogger(ScriptInterface& scriptInterface) :
	m_ScriptInterface(scriptInterface), m_Stream(NULL)
{
}

CReplayLogger::~CReplayLogger()
{
	delete m_Stream;
}

void CReplayLogger::StartGame(JS::MutableHandleValue attribs)
{
	// Add timestamp, since the file-modification-date can change
	m_ScriptInterface.SetProperty(attribs, "timestamp", (double)std::time(nullptr));

	// Add engine version and currently loaded mods for sanity checks when replaying
	m_ScriptInterface.SetProperty(attribs, "engine_version", CStr(engine_version));
	m_ScriptInterface.SetProperty(attribs, "mods", g_modsLoaded);

	m_Directory = createDateIndexSubdirectory(VisualReplay::GetDirectoryName());
	debug_printf("Writing replay to %s\n", m_Directory.string8().c_str());

	m_Stream = new std::ofstream(OsString(m_Directory / L"commands.txt").c_str(), std::ofstream::out | std::ofstream::trunc);
	*m_Stream << "start " << m_ScriptInterface.StringifyJSON(attribs, false) << "\n";
}

void CReplayLogger::Turn(u32 n, u32 turnLength, std::vector<SimulationCommand>& commands)
{
	JSContext* cx = m_ScriptInterface.GetContext();
	JSAutoRequest rq(cx);

	*m_Stream << "turn " << n << " " << turnLength << "\n";

	for (SimulationCommand& command : commands)
		*m_Stream << "cmd " << command.player << " " << m_ScriptInterface.StringifyJSON(&command.data, false) << "\n";

	*m_Stream << "end\n";
	m_Stream->flush();
}

void CReplayLogger::Hash(const std::string& hash, bool quick)
{
	if (quick)
		*m_Stream << "hash-quick " << Hexify(hash) << "\n";
	else
		*m_Stream << "hash " << Hexify(hash) << "\n";
}

OsPath CReplayLogger::GetDirectory() const
{
	return m_Directory;
}

////////////////////////////////////////////////////////////////

CReplayPlayer::CReplayPlayer() :
	m_Stream(NULL)
{
}

CReplayPlayer::~CReplayPlayer()
{
	delete m_Stream;
}

void CReplayPlayer::Load(const OsPath& path)
{
	ENSURE(!m_Stream);

	m_Stream = new std::ifstream(OsString(path).c_str());
	ENSURE(m_Stream->good());
}

void CReplayPlayer::Replay(bool serializationtest, int rejointestturn, bool ooslog)
{
	ENSURE(m_Stream);

	new CProfileViewer;
	new CProfileManager;
	g_ScriptStatsTable = new CScriptStatsTable;
	g_ProfileViewer.AddRootTable(g_ScriptStatsTable);

	const int runtimeSize = 384 * 1024 * 1024;
	const int heapGrowthBytesGCTrigger = 20 * 1024 * 1024;
	g_ScriptRuntime = ScriptInterface::CreateRuntime(shared_ptr<ScriptRuntime>(), runtimeSize, heapGrowthBytesGCTrigger);

	g_Game = new CGame(true, false);
	if (serializationtest)
		g_Game->GetSimulation2()->EnableSerializationTest();
	if (rejointestturn > 0)
		g_Game->GetSimulation2()->EnableRejoinTest(rejointestturn);
	if (ooslog)
		g_Game->GetSimulation2()->EnableOOSLog();

	// Need some stuff for terrain movement costs:
	// (TODO: this ought to be independent of any graphics code)
	new CTerrainTextureManager;
	g_TexMan.LoadTerrainTextures();

	// Initialise h_mgr so it doesn't crash when emitting sounds
	h_mgr_init();

	std::vector<SimulationCommand> commands;
	u32 turn = 0;
	u32 turnLength = 0;

	{
	JSContext* cx = g_Game->GetSimulation2()->GetScriptInterface().GetContext();
	JSAutoRequest rq(cx);
	std::string type;
	while ((*m_Stream >> type).good())
	{
		if (type == "start")
		{
			std::string line;
			std::getline(*m_Stream, line);
			JS::RootedValue attribs(cx);
			ENSURE(g_Game->GetSimulation2()->GetScriptInterface().ParseJSON(line, &attribs));

			std::vector<CStr> replayModList;
			g_Game->GetSimulation2()->GetScriptInterface().GetProperty(attribs, "mods", replayModList);

			for (const CStr& mod : replayModList)
				if (mod != "user" && std::find(g_modsLoaded.begin(), g_modsLoaded.end(), mod) == g_modsLoaded.end())
					LOGWARNING("The mod '%s' is required by the replay file, but wasn't passed as an argument!", mod);

			for (const CStr& mod : g_modsLoaded)
				if (mod != "user" && std::find(replayModList.begin(), replayModList.end(), mod) == replayModList.end())
					LOGWARNING("The mod '%s' wasn't used when creating this replay file, but was passed as an argument!", mod);

			g_Game->StartGame(&attribs, "");

			// TODO: Non progressive load can fail - need a decent way to handle this
			LDR_NonprogressiveLoad();

			PSRETURN ret = g_Game->ReallyStartGame();
			ENSURE(ret == PSRETURN_OK);
		}
		else if (type == "turn")
		{
			*m_Stream >> turn >> turnLength;
			debug_printf("Turn %u (%u)...\n", turn, turnLength);
		}
		else if (type == "cmd")
		{
			player_id_t player;
			*m_Stream >> player;

			std::string line;
			std::getline(*m_Stream, line);
			JS::RootedValue data(cx);
			g_Game->GetSimulation2()->GetScriptInterface().ParseJSON(line, &data);
			g_Game->GetSimulation2()->GetScriptInterface().FreezeObject(data, true);
			commands.emplace_back(SimulationCommand(player, cx, data));
		}
		else if (type == "hash" || type == "hash-quick")
		{
			std::string replayHash;
			*m_Stream >> replayHash;

			bool quick = (type == "hash-quick");

			if (turn % 100 == 0)
			{
				std::string hash;
				bool ok = g_Game->GetSimulation2()->ComputeStateHash(hash, quick);
				ENSURE(ok);
				std::string hexHash = Hexify(hash);
				if (hexHash == replayHash)
					debug_printf("hash ok (%s)\n", hexHash.c_str());
				else
					debug_printf("HASH MISMATCH (%s != %s)\n", hexHash.c_str(), replayHash.c_str());
			}
		}
		else if (type == "end")
		{
			{
				g_Profiler2.RecordFrameStart();
				PROFILE2("frame");
				g_Profiler2.IncrementFrameNumber();
				PROFILE2_ATTR("%d", g_Profiler2.GetFrameNumber());

				g_Game->GetSimulation2()->Update(turnLength, commands);
				commands.clear();
			}

			g_Profiler.Frame();

			if (turn % 20 == 0)
				g_ProfileViewer.SaveToFile();
		}
		else
		{
			debug_printf("Unrecognised replay token %s\n", type.c_str());
		}
	}
	}

	SAFE_DELETE(m_Stream);

	g_Profiler2.SaveToFile();

	std::string hash;
	bool ok = g_Game->GetSimulation2()->ComputeStateHash(hash, false);
	ENSURE(ok);
	debug_printf("# Final state: %s\n", Hexify(hash).c_str());
	timer_DisplayClientTotals();

	SAFE_DELETE(g_Game);

	// Must be explicitly destructed here to avoid callbacks from the JSAPI trying to use g_Profiler2 when
	// it's already destructed.
	g_ScriptRuntime.reset();

	// Clean up
	delete &g_TexMan;

	delete &g_Profiler;
	delete &g_ProfileViewer;
	SAFE_DELETE(g_ScriptStatsTable);
}
