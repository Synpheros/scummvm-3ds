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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

// Disable symbol overrides so that we can use system headers.
#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "backends/text-to-speech/linux/linux-text-to-speech.h"

#if defined(USE_LINUX_TTS)
#include <speech-dispatcher/libspeechd.h>

#include "common/translation.h"
#include "common/debug.h"
#include "common/system.h"
SPDConnection *_connection;

void speech_begin_callback(size_t msg_id, size_t client_id, SPDNotificationType state){
	LinuxTextToSpeechManager *manager =
		static_cast<LinuxTextToSpeechManager *> (g_system->getTextToSpeechManager());
	manager->updateState(LinuxTextToSpeechManager::SPEAKING);
}

void speech_end_callback(size_t msg_id, size_t client_id, SPDNotificationType state){
	LinuxTextToSpeechManager *manager =
		static_cast<LinuxTextToSpeechManager *> (g_system->getTextToSpeechManager());
	manager->updateState(LinuxTextToSpeechManager::READY);
}

void speech_cancel_callback(size_t msg_id, size_t client_id, SPDNotificationType state){
	LinuxTextToSpeechManager *manager =
		static_cast<LinuxTextToSpeechManager *> (g_system->getTextToSpeechManager());
	manager->updateState(LinuxTextToSpeechManager::READY);
}

void speech_resume_callback(size_t msg_id, size_t client_id, SPDNotificationType state){
	LinuxTextToSpeechManager *manager =
		static_cast<LinuxTextToSpeechManager *> (g_system->getTextToSpeechManager());
	manager->updateState(LinuxTextToSpeechManager::SPEAKING);
}

void speech_pause_callback(size_t msg_id, size_t client_id, SPDNotificationType state){
	LinuxTextToSpeechManager *manager =
		static_cast<LinuxTextToSpeechManager *> (g_system->getTextToSpeechManager());
	manager->updateState(LinuxTextToSpeechManager::PAUSED);
}

LinuxTextToSpeechManager::LinuxTextToSpeechManager()
	: _speechState(READY) {
	_connection = spd_open("ScummVM", "main", NULL, SPD_MODE_THREADED);
	if (_connection == 0) {
		_speechState = BROKEN;
		return;
	}

	_connection->callback_begin = speech_begin_callback;
	spd_set_notification_on(_connection, SPD_BEGIN);
	_connection->callback_end = speech_end_callback;
	spd_set_notification_on(_connection, SPD_END);
	_connection->callback_cancel = speech_cancel_callback;
	spd_set_notification_on(_connection, SPD_CANCEL);
	_connection->callback_resume = speech_resume_callback;
	spd_set_notification_on(_connection, SPD_RESUME);
	_connection->callback_pause = speech_pause_callback;
	spd_set_notification_on(_connection, SPD_PAUSE);

	setLanguage(Common::String("cs"));
	updateVoices();
}

LinuxTextToSpeechManager::~LinuxTextToSpeechManager() {
	if (_connection != 0)
		spd_close(_connection);
}

void LinuxTextToSpeechManager::updateState(LinuxTextToSpeechManager::SpeechState state) {
	_speechState = state;
}

bool LinuxTextToSpeechManager::say(Common::String str) {
	if (_speechState == BROKEN)
		return true;
	if (isSpeaking())
		stop();
	return spd_say(_connection, SPD_MESSAGE, str.c_str()) == -1;

}

bool LinuxTextToSpeechManager::stop() {
	if (_speechState == READY || _speechState == BROKEN)
		return true;
	return spd_cancel(_connection) == -1;
}

bool LinuxTextToSpeechManager::pause() {
	if (_speechState == READY || _speechState == PAUSED || _speechState == BROKEN)
		return true;
	return spd_pause(_connection) == -1;
}

bool LinuxTextToSpeechManager::resume() {
	if (_speechState == READY || _speechState == SPEAKING || _speechState == BROKEN)
		return true;
	return spd_resume(_connection) == -1;
}

bool LinuxTextToSpeechManager::isSpeaking() {
	return _speechState == SPEAKING;
}

bool LinuxTextToSpeechManager::isPaused() {
	return _speechState == PAUSED;
}

bool LinuxTextToSpeechManager::isReady() {
	return _speechState == READY;
}

void LinuxTextToSpeechManager::setVoice(Common::TTSVoice *voice) {
	if (_speechState == BROKEN)
		return;
	assert(voice != nullptr && voice->getData() != nullptr);
	spd_set_voice_type(_connection, *(SPDVoiceType *)(voice->getData()));
	_ttsState->_activeVoice = voice;
}

void LinuxTextToSpeechManager::setRate(int rate) {
	if (_speechState == BROKEN)
		return;
	assert(rate >= -100 && rate <= 100);
	spd_set_voice_rate(_connection, rate);
	_ttsState->_rate = rate;
}

void LinuxTextToSpeechManager::setPitch(int pitch) {
	if (_speechState == BROKEN)
		return;
	assert(pitch >= -100 && pitch <= 100);
	spd_set_voice_pitch(_connection, pitch);
	_ttsState->_pitch = pitch;
}

void LinuxTextToSpeechManager::setVolume(int volume) {
	if (_speechState == BROKEN)
		return;
	assert(volume >= -100 && volume <= 100);
	spd_set_volume(_connection, volume);
	_ttsState->_volume = volume;
}

void LinuxTextToSpeechManager::setLanguage(Common::String language) {
	if (_speechState == BROKEN)
		return;
	spd_set_language(_connection, language.c_str());
	_ttsState->_language = language;
	if (_ttsState->_activeVoice)
		setVoice(_ttsState->_activeVoice);
}

void LinuxTextToSpeechManager::updateVoices() {
	if (_speechState == BROKEN)
		return;
	/* just use these voices:
	   SPD_MALE1, SPD_MALE2, SPD_MALE3,
	   SPD_FEMALE1, SPD_FEMALE2, SPD_FEMALE3,
	   SPD_CHILD_MALE, SPD_CHILD_FEMALE

	   it depends on the user to map them to the right voices in speech-dispatcher
	   configuration
	*/

	SPDVoiceType *type = (SPDVoiceType *) malloc(sizeof(SPDVoiceType));
	*type = SPD_MALE1;
	_ttsState->_availaibleVoices.push_back(Common::TTSVoice(Common::TTSVoice::MALE, (void *) type));

	type = (SPDVoiceType *) malloc(sizeof(SPDVoiceType));
	*type = SPD_MALE2;
	_ttsState->_availaibleVoices.push_back(Common::TTSVoice(Common::TTSVoice::MALE, (void *) type));

	type = (SPDVoiceType *) malloc(sizeof(SPDVoiceType));
	*type = SPD_MALE3;
	_ttsState->_availaibleVoices.push_back(Common::TTSVoice(Common::TTSVoice::MALE, (void *) type));

	type = (SPDVoiceType *) malloc(sizeof(SPDVoiceType));
	*type = SPD_FEMALE1;
	_ttsState->_availaibleVoices.push_back(Common::TTSVoice(Common::TTSVoice::FEMALE, (void *) type));

	type = (SPDVoiceType *) malloc(sizeof(SPDVoiceType));
	*type = SPD_FEMALE2;
	_ttsState->_availaibleVoices.push_back(Common::TTSVoice(Common::TTSVoice::FEMALE, (void *) type));

	type = (SPDVoiceType *) malloc(sizeof(SPDVoiceType));
	*type = SPD_FEMALE3;
	_ttsState->_availaibleVoices.push_back(Common::TTSVoice(Common::TTSVoice::FEMALE, (void *) type));

	type = (SPDVoiceType *) malloc(sizeof(SPDVoiceType));
	*type = SPD_CHILD_MALE;
	_ttsState->_availaibleVoices.push_back(Common::TTSVoice(Common::TTSVoice::MALE, (void *) type));

	type = (SPDVoiceType *) malloc(sizeof(SPDVoiceType));
	*type = SPD_CHILD_FEMALE;
	_ttsState->_availaibleVoices.push_back(Common::TTSVoice(Common::TTSVoice::FEMALE, (void *) type));
}

#endif