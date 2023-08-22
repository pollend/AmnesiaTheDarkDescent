/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "LuxBase.h"
#include <mutex>


class cLuxSaveGame_SaveData;


class cLuxSaveHandler : public iLuxUpdateable
{
public:
	cLuxSaveHandler();
	~cLuxSaveHandler();

	void OnStart();
	void Update(float afTimeStep);
	void Reset();

	void SaveGameToFile(const tWString& asFile, bool abSaveSnapshot=false);
	void LoadGameFromFile(const tWString& asFile);

	bool AutoSave();
	bool AutoLoad(bool abResetProgressLogger);

	bool SaveFileExists();

	bool HardModeSave();

	cLuxSaveGame_SaveData *CreateSaveGameData();
	void LoadSaveGameData(cLuxSaveGame_SaveData *apSave);

	tWString GetProperSaveName(const tWString& asFile);

private:
	tWString GetSaveName(const tWString &asPrefix);
	void DeleteOldestSaveFiles(const tWString &asFolder, int alMax);
	tWString GetNewestSaveFile(const tWString &asFolder);

	cDate mLatestSaveDate;
	int mlMaxAutoSaves;
	int mlSaveNameCount;
};

