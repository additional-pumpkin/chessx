/***************************************************************************
                          playerinfo  -  description
                             -------------------
    copyright            : (C) 2007 Michal Rudolf <mrudolf@kdewebdev.org>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef PLAYERINFO_H
#define PLAYERINFO_H

#include "common.h"

#include <QString>
#include <QList>
#include <QPair>

class Database;

class PlayerInfo{
public:
	/** Standard constructor. Does nothing. */
	PlayerInfo();
	/** Set database and player. Automatically updates information. */
	PlayerInfo(Database* db, const QString& player);
	/** Standard destructor. */
	~PlayerInfo();
	/** Changes database. Does not automatically update statistics. */
	void setDatabase(Database* db);
	/** Changes player. Does not automatically update statistics. */
	void setName(const QString& player);
	/** @return current player name. */
	QString name() const;
	/** Recalculates all statistics. */
	void update();
private:
	/** Clears all statistics. */
	void reset();
	/** Add statistics for game @p index when player has given color. */
	void addGameStats(int index, Color color);

	enum StatColor {White, Black, AnyColor};
	enum StatResult {Unknown, WhiteWin, Draw, BlackWin, AnyResult};
	typedef QList<QPair<QString, unsigned> > EcoFrequency;
	QString m_name;
	Database* m_database;
	unsigned m_results[AnyColor+1][AnyResult+1];
	EcoFrequency m_eco[2];
};

#endif