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

#include "dreamweb/dreamweb.h"

namespace DreamGen {

void DreamBase::talk() {
	_talkPos = 0;
	_inMapArea = 0;
	_character = _command;
	createPanel();
	showPanel();
	showMan();
	showExit();
	underTextLine();
	convIcons();
	startTalk();
	_commandType = 255;
	readMouse();
	showPointer();
	workToScreen();

	RectWithCallback<DreamBase> talkList[] = {
		{ 273,320,157,198,&DreamBase::getBack1 },
		{ 240,290,2,44,&DreamBase::moreTalk },
		{ 0,320,0,200,&DreamBase::blank },
		{ 0xFFFF,0,0,0,0 }
	};

	do {
		delPointer();
		readMouse();
		animPointer();
		showPointer();
		vSync();
		dumpPointer();
		dumpTextLine();
		_getBack = 0;
		checkCoords(talkList);
		if (_quitRequested)
			break;
	} while (!_getBack);

	if (_talkPos >= 4)
		_personData->b7 |= 128;

	redrawMainScrn();
	workToScreenM();
	if (_speechLoaded) {
		cancelCh1();
		_volumeDirection = -1;
		_volumeTo = 0;
	}
}

void DreamBase::convIcons() {
	uint8 index = _character & 127;
	uint16 frame = getPersFrame(index);
	const GraphicsFile *base = findSource(frame);
	showFrame(*base, 234, 2, frame, 0);
}

uint16 DreamBase::getPersFrame(uint8 index) {
	return READ_LE_UINT16(&_personFramesLE[index]);
}

void DreamBase::startTalk() {
	_talkMode = 0;

	const uint8 *str = getPersonText(_character & 0x7F, 0);
	uint16 y;

	_charShift = 91+91;
	y = 64;
	printDirect(&str, 66, &y, 241, true);

	_charShift = 0;
	y = 80;
	printDirect(&str, 66, &y, 241, true);

	_speechLoaded = false;
	loadSpeech('R', _realLocation, 'C', 64*(_character & 0x7F));
	if (_speechLoaded) {
		_volumeDirection = 1;
		_volumeTo = 6;
		playChannel1(50 + 12);
	}
}

const uint8 *DreamBase::getPersonText(uint8 index, uint8 talkPos) {
	return (const uint8 *)_personText.getString(index*64 + talkPos);
}

void DreamBase::moreTalk() {
	if (_talkMode != 0) {
		redes();
		return;
	}

	if (_commandType != 215) {
		_commandType = 215;
		commandOnly(49);
	}

	if (_mouseButton == _oldButton)
		return;	// nomore

	if (!(_mouseButton & 1))
		return;

	_talkMode = 2;
	_talkPos = 4;

	if (_character >= 100)
		_talkPos = 48; // second part
	doSomeTalk();
}

void DreamBase::doSomeTalk() {
	while (true) {
		const uint8 *str = getPersonText(_character & 0x7F, _talkPos);

		if (*str == 0) {
			// endheartalk
			_pointerMode = 0;
			return;
		}

		createPanel();
		showPanel();
		showMan();
		showExit();
		convIcons();

		printDirect(str, 164, 64, 144, false);

		loadSpeech('R', _realLocation, 'C', (64 * (_character & 0x7F)) + _talkPos);
		if (_speechLoaded)
			playChannel1(62);

		_pointerMode = 3;
		workToScreenM();
		if (hangOnPQ())
			return;

		_talkPos++;

		str = getPersonText(_character & 0x7F, _talkPos);
		if (*str == 0) {
			// endheartalk
			_pointerMode = 0;
			return;
		}

		if (*str != ':' && *str != 32) {
			createPanel();
			showPanel();
			showMan();
			showExit();
			convIcons();
			printDirect(str, 48, 128, 144, false);

			loadSpeech('R', _realLocation, 'C', (64 * (_character & 0x7F)) + _talkPos);
			if (_speechLoaded)
				playChannel1(62);

			_pointerMode = 3;
			workToScreenM();
			if (hangOnPQ())
				return;
		}

		_talkPos++;
	}
}

bool DreamBase::hangOnPQ() {
	_getBack = 0;

	RectWithCallback<DreamBase> quitList[] = {
		{ 273,320,157,198,&DreamBase::getBack1 },
		{ 0,320,0,200,&DreamBase::blank },
		{ 0xFFFF,0,0,0,0 }
	};

	uint16 speechFlag = 0;

	do {
		delPointer();
		readMouse();
		animPointer();
		showPointer();
		vSync();
		dumpPointer();
		dumpTextLine();
		checkCoords(quitList);

		if (_getBack == 1 || _quitRequested) {
			// Quit conversation
			delPointer();
			_pointerMode = 0;
			cancelCh1();
			return true;
		}

		if (_speechLoaded && _channel1Playing == 255) {
			speechFlag++;
			if (speechFlag == 40)
				break;
		}
	} while (!_mouseButton || _oldButton);

	delPointer();
	_pointerMode = 0;
	return false;
}

void DreamBase::redes() {
	if (_channel1Playing != 255 || _talkMode != 2) {
		blank();
		return;
	}

	if (_commandType != 217) {
		_commandType = 217;
		commandOnly(50);
	}

	if (!(_mouseButton & 1))
		return;

	delPointer();
	createPanel();
	showPanel();
	showMan();
	showExit();
	convIcons();
	startTalk();
	readMouse();
	showPointer();
	workToScreen();
	delPointer();
}

} // End of namespace DreamGen
