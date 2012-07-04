/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

/*
 * This file is based on WME Lite.
 * http://dead-code.org/redir.php?target=wmelite
 * Copyright (c) 2011 Jan Nedoma
 */

#include "engines/wintermute/dcgf.h"
#include "engines/wintermute/Base/BRegion.h"
#include "engines/wintermute/Base/BParser.h"
#include "engines/wintermute/Base/BDynBuffer.h"
#include "engines/wintermute/Base/BGame.h"
#include "engines/wintermute/Base/scriptables/ScScript.h"
#include "engines/wintermute/Base/scriptables/ScStack.h"
#include "engines/wintermute/Base/scriptables/ScValue.h"
#include "engines/wintermute/Base/BFileManager.h"
#include "engines/wintermute/PlatformSDL.h"
#include <limits.h>

namespace WinterMute {

IMPLEMENT_PERSISTENT(CBRegion, false)

//////////////////////////////////////////////////////////////////////////
CBRegion::CBRegion(CBGame *inGame): CBObject(inGame) {
	_active = true;
	_editorSelectedPoint = -1;
	_lastMimicScale = -1;
	_lastMimicX = _lastMimicY = INT_MIN;

	CBPlatform::SetRectEmpty(&_rect);
}


//////////////////////////////////////////////////////////////////////////
CBRegion::~CBRegion() {
	cleanup();
}


//////////////////////////////////////////////////////////////////////////
void CBRegion::cleanup() {
	for (int i = 0; i < _points.GetSize(); i++) delete _points[i];
	_points.RemoveAll();

	CBPlatform::SetRectEmpty(&_rect);
	_editorSelectedPoint = -1;
}


//////////////////////////////////////////////////////////////////////////
bool CBRegion::CreateRegion() {
	return SUCCEEDED(GetBoundingRect(&_rect));
}


//////////////////////////////////////////////////////////////////////////
bool CBRegion::PointInRegion(int X, int Y) {
	if (_points.GetSize() < 3) return false;

	POINT pt;
	pt.x = X;
	pt.y = Y;

	RECT rect;
	rect.left = X - 1;
	rect.right = X + 2;
	rect.top = Y - 1;
	rect.bottom = Y + 2;

	if (CBPlatform::PtInRect(&_rect, pt)) return PtInPolygon(X, Y);
	else return false;
}


//////////////////////////////////////////////////////////////////////////
HRESULT CBRegion::loadFile(const char *filename) {
	byte *Buffer = Game->_fileManager->readWholeFile(filename);
	if (Buffer == NULL) {
		Game->LOG(0, "CBRegion::LoadFile failed for file '%s'", filename);
		return E_FAIL;
	}

	HRESULT ret;

	_filename = new char [strlen(filename) + 1];
	strcpy(_filename, filename);

	if (FAILED(ret = loadBuffer(Buffer, true))) Game->LOG(0, "Error parsing REGION file '%s'", filename);


	delete [] Buffer;

	return ret;
}


TOKEN_DEF_START
TOKEN_DEF(REGION)
TOKEN_DEF(TEMPLATE)
TOKEN_DEF(NAME)
TOKEN_DEF(ACTIVE)
TOKEN_DEF(POINT)
TOKEN_DEF(CAPTION)
TOKEN_DEF(SCRIPT)
TOKEN_DEF(EDITOR_SELECTED_POINT)
TOKEN_DEF(PROPERTY)
TOKEN_DEF_END
//////////////////////////////////////////////////////////////////////////
HRESULT CBRegion::loadBuffer(byte  *Buffer, bool Complete) {
	TOKEN_TABLE_START(commands)
	TOKEN_TABLE(REGION)
	TOKEN_TABLE(TEMPLATE)
	TOKEN_TABLE(NAME)
	TOKEN_TABLE(ACTIVE)
	TOKEN_TABLE(POINT)
	TOKEN_TABLE(CAPTION)
	TOKEN_TABLE(SCRIPT)
	TOKEN_TABLE(EDITOR_SELECTED_POINT)
	TOKEN_TABLE(PROPERTY)
	TOKEN_TABLE_END

	byte *params;
	int cmd;
	CBParser parser(Game);

	if (Complete) {
		if (parser.GetCommand((char **)&Buffer, commands, (char **)&params) != TOKEN_REGION) {
			Game->LOG(0, "'REGION' keyword expected.");
			return E_FAIL;
		}
		Buffer = params;
	}

	int i;

	for (i = 0; i < _points.GetSize(); i++) delete _points[i];
	_points.RemoveAll();

	while ((cmd = parser.GetCommand((char **)&Buffer, commands, (char **)&params)) > 0) {
		switch (cmd) {
		case TOKEN_TEMPLATE:
			if (FAILED(loadFile((char *)params))) cmd = PARSERR_GENERIC;
			break;

		case TOKEN_NAME:
			setName((char *)params);
			break;

		case TOKEN_CAPTION:
			setCaption((char *)params);
			break;

		case TOKEN_ACTIVE:
			parser.ScanStr((char *)params, "%b", &_active);
			break;

		case TOKEN_POINT: {
			int x, y;
			parser.ScanStr((char *)params, "%d,%d", &x, &y);
			_points.Add(new CBPoint(x, y));
		}
		break;

		case TOKEN_SCRIPT:
			addScript((char *)params);
			break;

		case TOKEN_EDITOR_SELECTED_POINT:
			parser.ScanStr((char *)params, "%d", &_editorSelectedPoint);
			break;

		case TOKEN_PROPERTY:
			parseProperty(params, false);
			break;
		}
	}
	if (cmd == PARSERR_TOKENNOTFOUND) {
		Game->LOG(0, "Syntax error in REGION definition");
		return E_FAIL;
	}

	CreateRegion();

	return S_OK;
}


//////////////////////////////////////////////////////////////////////////
// high level scripting interface
//////////////////////////////////////////////////////////////////////////
HRESULT CBRegion::scCallMethod(CScScript *script, CScStack *stack, CScStack *thisStack, const char *name) {

	//////////////////////////////////////////////////////////////////////////
	// AddPoint
	//////////////////////////////////////////////////////////////////////////
	if (strcmp(name, "AddPoint") == 0) {
		stack->correctParams(2);
		int X = stack->pop()->getInt();
		int Y = stack->pop()->getInt();

		_points.Add(new CBPoint(X, Y));
		CreateRegion();

		stack->pushBool(true);

		return S_OK;
	}

	//////////////////////////////////////////////////////////////////////////
	// InsertPoint
	//////////////////////////////////////////////////////////////////////////
	else if (strcmp(name, "InsertPoint") == 0) {
		stack->correctParams(3);
		int Index = stack->pop()->getInt();
		int X = stack->pop()->getInt();
		int Y = stack->pop()->getInt();

		if (Index >= 0 && Index < _points.GetSize()) {
			_points.InsertAt(Index, new CBPoint(X, Y));
			CreateRegion();

			stack->pushBool(true);
		} else stack->pushBool(false);

		return S_OK;
	}

	//////////////////////////////////////////////////////////////////////////
	// SetPoint
	//////////////////////////////////////////////////////////////////////////
	else if (strcmp(name, "SetPoint") == 0) {
		stack->correctParams(3);
		int Index = stack->pop()->getInt();
		int X = stack->pop()->getInt();
		int Y = stack->pop()->getInt();

		if (Index >= 0 && Index < _points.GetSize()) {
			_points[Index]->x = X;
			_points[Index]->y = Y;
			CreateRegion();

			stack->pushBool(true);
		} else stack->pushBool(false);

		return S_OK;
	}

	//////////////////////////////////////////////////////////////////////////
	// RemovePoint
	//////////////////////////////////////////////////////////////////////////
	else if (strcmp(name, "RemovePoint") == 0) {
		stack->correctParams(1);
		int Index = stack->pop()->getInt();

		if (Index >= 0 && Index < _points.GetSize()) {
			delete _points[Index];
			_points[Index] = NULL;

			_points.RemoveAt(Index);
			CreateRegion();

			stack->pushBool(true);
		} else stack->pushBool(false);

		return S_OK;
	}

	//////////////////////////////////////////////////////////////////////////
	// GetPoint
	//////////////////////////////////////////////////////////////////////////
	else if (strcmp(name, "GetPoint") == 0) {
		stack->correctParams(1);
		int Index = stack->pop()->getInt();

		if (Index >= 0 && Index < _points.GetSize()) {
			CScValue *Val = stack->getPushValue();
			if (Val) {
				Val->setProperty("X", _points[Index]->x);
				Val->setProperty("Y", _points[Index]->y);
			}
		} else stack->pushNULL();

		return S_OK;
	}

	else return CBObject::scCallMethod(script, stack, thisStack, name);
}


//////////////////////////////////////////////////////////////////////////
CScValue *CBRegion::scGetProperty(const char *name) {
	_scValue->setNULL();

	//////////////////////////////////////////////////////////////////////////
	// Type
	//////////////////////////////////////////////////////////////////////////
	if (strcmp(name, "Type") == 0) {
		_scValue->setString("region");
		return _scValue;
	}

	//////////////////////////////////////////////////////////////////////////
	// Name
	//////////////////////////////////////////////////////////////////////////
	else if (strcmp(name, "Name") == 0) {
		_scValue->setString(_name);
		return _scValue;
	}

	//////////////////////////////////////////////////////////////////////////
	// Active
	//////////////////////////////////////////////////////////////////////////
	else if (strcmp(name, "Active") == 0) {
		_scValue->setBool(_active);
		return _scValue;
	}

	//////////////////////////////////////////////////////////////////////////
	// NumPoints
	//////////////////////////////////////////////////////////////////////////
	else if (strcmp(name, "NumPoints") == 0) {
		_scValue->setInt(_points.GetSize());
		return _scValue;
	}

	else return CBObject::scGetProperty(name);
}


//////////////////////////////////////////////////////////////////////////
HRESULT CBRegion::scSetProperty(const char *name, CScValue *value) {
	//////////////////////////////////////////////////////////////////////////
	// Name
	//////////////////////////////////////////////////////////////////////////
	if (strcmp(name, "Name") == 0) {
		setName(value->getString());
		return S_OK;
	}

	//////////////////////////////////////////////////////////////////////////
	// Active
	//////////////////////////////////////////////////////////////////////////
	else if (strcmp(name, "Active") == 0) {
		_active = value->getBool();
		return S_OK;
	}

	else return CBObject::scSetProperty(name, value);
}


//////////////////////////////////////////////////////////////////////////
const char *CBRegion::scToString() {
	return "[region]";
}


//////////////////////////////////////////////////////////////////////////
HRESULT CBRegion::saveAsText(CBDynBuffer *Buffer, int Indent, const char *NameOverride) {
	if (!NameOverride) Buffer->putTextIndent(Indent, "REGION {\n");
	else Buffer->putTextIndent(Indent, "%s {\n", NameOverride);

	Buffer->putTextIndent(Indent + 2, "NAME=\"%s\"\n", _name);
	Buffer->putTextIndent(Indent + 2, "CAPTION=\"%s\"\n", getCaption());
	Buffer->putTextIndent(Indent + 2, "ACTIVE=%s\n", _active ? "TRUE" : "FALSE");
	Buffer->putTextIndent(Indent + 2, "EDITOR_SELECTED_POINT=%d\n", _editorSelectedPoint);

	int i;

	for (i = 0; i < _scripts.GetSize(); i++) {
		Buffer->putTextIndent(Indent + 2, "SCRIPT=\"%s\"\n", _scripts[i]->_filename);
	}

	for (i = 0; i < _points.GetSize(); i++) {
		Buffer->putTextIndent(Indent + 2, "POINT {%d,%d}\n", _points[i]->x, _points[i]->y);
	}

	if (_scProp) _scProp->saveAsText(Buffer, Indent + 2);

	Buffer->putTextIndent(Indent, "}\n\n");

	return S_OK;
}


//////////////////////////////////////////////////////////////////////////
HRESULT CBRegion::persist(CBPersistMgr *persistMgr) {

	CBObject::persist(persistMgr);

	persistMgr->transfer(TMEMBER(_active));
	persistMgr->transfer(TMEMBER(_editorSelectedPoint));
	persistMgr->transfer(TMEMBER(_lastMimicScale));
	persistMgr->transfer(TMEMBER(_lastMimicX));
	persistMgr->transfer(TMEMBER(_lastMimicY));
	_points.persist(persistMgr);

	return S_OK;
}


typedef struct {
	double x, y;
} dPoint;

//////////////////////////////////////////////////////////////////////////
bool CBRegion::PtInPolygon(int X, int Y) {
	if (_points.GetSize() < 3) return false;

	int counter = 0;
	int i;
	double xinters;
	dPoint p, p1, p2;

	p.x = (double)X;
	p.y = (double)Y;

	p1.x = (double)_points[0]->x;
	p1.y = (double)_points[0]->y;

	for (i = 1; i <= _points.GetSize(); i++) {
		p2.x = (double)_points[i % _points.GetSize()]->x;
		p2.y = (double)_points[i % _points.GetSize()]->y;

		if (p.y > MIN(p1.y, p2.y)) {
			if (p.y <= MAX(p1.y, p2.y)) {
				if (p.x <= MAX(p1.x, p2.x)) {
					if (p1.y != p2.y) {
						xinters = (p.y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y) + p1.x;
						if (p1.x == p2.x || p.x <= xinters)
							counter++;
					}
				}
			}
		}
		p1 = p2;
	}

	if (counter % 2 == 0)
		return false;
	else
		return true;
}


//////////////////////////////////////////////////////////////////////////
HRESULT CBRegion::GetBoundingRect(RECT *Rect) {
	if (_points.GetSize() == 0) CBPlatform::SetRectEmpty(Rect);
	else {
		int MinX = INT_MAX, MinY = INT_MAX, MaxX = INT_MIN, MaxY = INT_MIN;

		for (int i = 0; i < _points.GetSize(); i++) {
			MinX = MIN(MinX, _points[i]->x);
			MinY = MIN(MinY, _points[i]->y);

			MaxX = MAX(MaxX, _points[i]->x);
			MaxY = MAX(MaxY, _points[i]->y);
		}
		CBPlatform::SetRect(Rect, MinX, MinY, MaxX, MaxY);
	}
	return S_OK;
}


//////////////////////////////////////////////////////////////////////////
HRESULT CBRegion::Mimic(CBRegion *Region, float Scale, int X, int Y) {
	if (Scale == _lastMimicScale && X == _lastMimicX && Y == _lastMimicY) return S_OK;

	cleanup();

	for (int i = 0; i < Region->_points.GetSize(); i++) {
		int x, y;

		x = (int)((float)Region->_points[i]->x * Scale / 100.0f);
		y = (int)((float)Region->_points[i]->y * Scale / 100.0f);

		_points.Add(new CBPoint(x + X, y + Y));
	}

	_lastMimicScale = Scale;
	_lastMimicX = X;
	_lastMimicY = Y;

	return CreateRegion() ? S_OK : E_FAIL;
}

} // end of namespace WinterMute
