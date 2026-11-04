/*
 * Copyright (C) 2026 Azkali Manad <a.ffcc7@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>

#include "MediatekRadio.h"
#include "common.h"

#define FM_DEV_NAME "/dev/fm"

// The driver ignores the band, its range is fixed to 87.5 - 108 MHz
#define FM_BAND_UE      1

#define FM_ANTENNA_LONG  0 // headset wire
#define FM_ANTENNA_SHORT 1 // embedded antenna

// Frequencies are in 10 kHz units
static const int kBandMin = 8750;
static const int kBandMax = 10800;
static const int kStep = 10;
static const int kRdsInterval = 100;
static const int kDefaultFrequency = 10250;
static const int kPresetCount = 4;

// Node names differ between pulseaudio-modules-droid releases:
//   Halium 9/10: source.droid / sink.droid
//   Halium 11+:  source.primary_input / sink.primary_output
static QString findNode(const QStringList &names, const QStringList &candidates)
{
	for (const QString &candidate : candidates)
		if (names.contains(candidate))
			return candidate;

	return candidates.first();
}

// Wrap around at the band edges
static int wrapFrequency(int freq)
{
	if (freq > kBandMax)
		return kBandMin;
	if (freq < kBandMin)
		return kBandMax;

	return freq;
}

MediatekRadio::MediatekRadio(QObject *parent)
	: QObject(parent),
	  settings(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/radio.conf",
		   QSettings::IniFormat),
	  recorder(pulse),
	  paSource(findNode(pulse.sourceNames(), {"source.primary_input", "source.droid"})),
	  paSink(findNode(pulse.sinkNames(), {"sink.primary_output", "sink.droid"})),
	  headsetAntenna(settings.value("headsetAntenna", true).toBool()),
	  mono(settings.value("mono", false).toBool()),
	  region(static_cast<Region>(settings.value("region", RegionWorld).toInt())),
	  rdsEnabled(settings.value("rds", true).toBool()),
	  frequency(settings.value("frequency", kDefaultFrequency).toInt())
{
	// An empty slot is stored as 0
	presets = settings.value("presets").toList();
	while (presets.size() < kPresetCount)
		presets << 0;

	connect(&recorder, &PulseRecorder::recordingChanged, this, [this]() {
		if (!recorder.isRecording())
			recordingTimer.stop();
		emit recordingChanged();
		emit recordingDurationChanged();
	});

	recordingTimer.setInterval(1000);
	connect(&recordingTimer, &QTimer::timeout, this, &MediatekRadio::recordingDurationChanged);

	rdsTimer.setInterval(kRdsInterval);
	connect(&rdsTimer, &QTimer::timeout, this, &MediatekRadio::readRds);
}

MediatekRadio::~MediatekRadio()
{
	stopRadio();
}

// Wired headsets and headphones act as the antenna
bool MediatekRadio::isHeadsetConnected()
{
	const QString port = pulse.sinkActivePort(paSink);

	return port == "output-wired_headset" || port == "output-wired_headphone";
}

void MediatekRadio::routeAudio()
{
	savedSourcePort = pulse.sourceActivePort(paSource);
	savedSinkPort = pulse.sinkActivePort(paSink);

	pulse.setSourcePort(paSource, "input-fm_tuner");
	if (speakerOutput)
		pulse.setSinkPort(paSink, "output-speaker");

	// The multimedia role makes the loopback follow the system media volume
	loopbackModule = pulse.loadModule("module-loopback",
					  QString("source=%1 sink=%2 sink_input_properties=media.role=multimedia")
						  .arg(paSource, paSink));
}

void MediatekRadio::restoreAudio()
{
	if (loopbackModule != PA_INVALID_INDEX)
		pulse.unloadModule(loopbackModule);
	loopbackModule = PA_INVALID_INDEX;

	if (savedSourcePort.isEmpty() || savedSourcePort == "input-fm_tuner")
		savedSourcePort = savedSinkPort == "output-wired_headset" ? "input-wired_headset" : "input-builtin_mic";

	pulse.setSourcePort(paSource, savedSourcePort);
	if (!savedSinkPort.isEmpty())
		pulse.setSinkPort(paSink, savedSinkPort);
}

// De-emphasis is only applied at power up
bool MediatekRadio::powerUp(int freq)
{
	int ret = COM_pwr_up(fd, FM_BAND_UE, freq, region == RegionAmericas);
	if (ret < 0) {
		qWarning() << "Failed to power up:" << ret;
		return false;
	}

	applyAntenna();
	applyMono();
	applyRds();

	return true;
}

QByteArray MediatekRadio::startRadio(int freq)
{
	if (running)
		return "Stop radio";

	if (headsetAntenna && !isHeadsetConnected())
		return "Headset not available";

	int ret = COM_open_dev(FM_DEV_NAME, &fd);
	if (ret < 0) {
		qWarning() << "Failed to open" << FM_DEV_NAME << ret;
		return "Error";
	}

	if (!powerUp(freq)) {
		COM_close_dev(fd);
		fd = -1;
		return "Error";
	}

	setFrequency(freq);
	routeAudio();

	running = true;
	emit runningChanged();

	return "Stop radio";
}

QByteArray MediatekRadio::stopRadio()
{
	if (!running)
		return "Start radio";

	stopRecording();
	rdsTimer.stop();
	clearRds();
	restoreAudio();

	int ret = COM_pwr_down(fd, 0);
	if (ret < 0)
		qWarning() << "Failed to power down:" << ret;

	COM_close_dev(fd);
	fd = -1;

	running = false;
	emit runningChanged();

	return "Start radio";
}

bool MediatekRadio::isRadioRunning() const
{
	return running;
}

void MediatekRadio::tune(int freq)
{
	if (running && COM_tune(fd, freq, FM_BAND_UE) < 0) {
		qWarning() << "Failed to tune to" << freq;
		return;
	}

	setFrequency(freq);
}

int MediatekRadio::getFrequency() const
{
	return frequency;
}

void MediatekRadio::setFrequency(int freq)
{
	if (frequency == freq)
		return;

	frequency = freq;
	settings.setValue("frequency", freq);
	clearRds();
	emit frequencyChanged();
}

int MediatekRadio::seekUp()
{
	return seek(true);
}

int MediatekRadio::seekDown()
{
	return seek(false);
}

int MediatekRadio::stepUp()
{
	return step(true);
}

int MediatekRadio::stepDown()
{
	return step(false);
}

// Move by 0.1 MHz
int MediatekRadio::step(bool up)
{
	tune(wrapFrequency(frequency + (up ? kStep : -kStep)));

	return frequency;
}

// Same as MTK's FM app: soft mute tune every channel until the chip reports a valid one
int MediatekRadio::seek(bool up)
{
	if (!running)
		return frequency;

	setMute(true);
	COM_pre_search(fd);

	const int channels = (kBandMax - kBandMin) / kStep;
	int found = frequency;
	int freq = frequency;

	for (int i = 0; i < channels; i++) {
		freq = wrapFrequency(freq + (up ? kStep : -kStep));

		fm_softmute_tune_t channel = {};
		channel.freq = freq;
		if (COM_Soft_Mute_Tune(fd, &channel) == 0 && channel.valid) {
			found = freq;
			break;
		}
	}

	COM_restore_search(fd);

	// Soft mute tune moves the chip, so tune back even if nothing was found
	if (COM_tune(fd, found, FM_BAND_UE) < 0)
		qWarning() << "Failed to tune to" << found;
	setFrequency(found);
	setMute(false);

	return frequency;
}

void MediatekRadio::setMute(bool mute)
{
	int ret = COM_set_mute(fd, mute);
	if (ret < 0)
		qWarning() << "Failed to set mute:" << ret;
}

QVariantList MediatekRadio::getPresets() const
{
	return presets;
}

void MediatekRadio::savePreset(int slot)
{
	setPreset(slot, frequency);
}

void MediatekRadio::clearPreset(int slot)
{
	setPreset(slot, 0);
}

void MediatekRadio::setPreset(int slot, int freq)
{
	if (slot < 0 || slot >= kPresetCount)
		return;

	presets[slot] = freq;
	settings.setValue("presets", presets);
	emit presetsChanged();
}

bool MediatekRadio::isSpeakerOutput() const
{
	return speakerOutput;
}

void MediatekRadio::setSpeakerOutput(bool speaker)
{
	if (speakerOutput == speaker)
		return;

	speakerOutput = speaker;
	if (running)
		pulse.setSinkPort(paSink, speaker ? QStringLiteral("output-speaker") : savedSinkPort);

	emit speakerOutputChanged();
}

void MediatekRadio::applyAntenna()
{
	int ret = COM_ana_switch(fd, headsetAntenna ? FM_ANTENNA_LONG : FM_ANTENNA_SHORT);
	if (ret < 0)
		qWarning() << "Failed to switch antenna:" << ret;
}

bool MediatekRadio::isMono() const
{
	return mono;
}

void MediatekRadio::setMono(bool enabled)
{
	if (mono == enabled)
		return;

	mono = enabled;
	settings.setValue("mono", enabled);
	if (running)
		applyMono();

	emit monoChanged();
}

void MediatekRadio::applyMono()
{
	int ret = COM_set_stereomono(fd, mono);
	if (ret < 0)
		qWarning() << "Failed to set mono:" << ret;
}

MediatekRadio::Region MediatekRadio::getRegion() const
{
	return region;
}

void MediatekRadio::setRegion(Region newRegion)
{
	if (region == newRegion)
		return;

	region = newRegion;
	settings.setValue("region", static_cast<int>(newRegion));

	if (running) {
		COM_pwr_down(fd, 0);
		if (!powerUp(frequency))
			stopRadio();
	}

	emit regionChanged();
}

bool MediatekRadio::isRdsEnabled() const
{
	return rdsEnabled;
}

void MediatekRadio::setRdsEnabled(bool enabled)
{
	if (rdsEnabled == enabled)
		return;

	rdsEnabled = enabled;
	settings.setValue("rds", enabled);
	if (running)
		applyRds();

	emit rdsEnabledChanged();
}

void MediatekRadio::applyRds()
{
	int ret = COM_turn_on_off_rds(fd, rdsEnabled ? FMR_RDS_ON : FMR_RDS_OFF);
	if (ret < 0)
		qWarning() << "Failed to switch rds:" << ret;

	if (rdsEnabled) {
		rdsTimer.start();
	} else {
		rdsTimer.stop();
		clearRds();
	}
}

// read() never blocks and clears the driver side data, so poll it like MTK does
void MediatekRadio::readRds()
{
	RDSData_Struct rds;
	uint16_t status = 0;

	if (COM_read_rds_data(fd, &rds, &status) < 0)
		return;

	bool changed = false;

	if (status & RDS_EVENT_PROGRAMNAME) {
		uint8_t *ps = rds.PS_Data.PS[3];
		COM_change_string(ps, 8);
		stationName = QString::fromLatin1(reinterpret_cast<const char *>(ps), 8).trimmed();
		changed = true;
	}

	if (status & RDS_EVENT_LAST_RADIOTEXT) {
		uint8_t *rt = rds.RT_Data.TextData[3];
		const int length = qMin<int>(rds.RT_Data.TextLength, 64);
		COM_change_string(rt, length);
		radioText = QString::fromLatin1(reinterpret_cast<const char *>(rt), length).trimmed();
		changed = true;
	}

	if (changed)
		emit rdsChanged();
}

void MediatekRadio::clearRds()
{
	if (stationName.isEmpty() && radioText.isEmpty())
		return;

	stationName.clear();
	radioText.clear();
	emit rdsChanged();
}

QString MediatekRadio::getStationName() const
{
	return stationName;
}

QString MediatekRadio::getRadioText() const
{
	return radioText;
}

bool MediatekRadio::isRecording() const
{
	return recorder.isRecording();
}

QString MediatekRadio::recordingsFolder() const
{
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/recordings";
	QDir().mkpath(dir);

	return dir;
}

QString MediatekRadio::startRecording()
{
	if (!running || isRecording())
		return QString();

	const QString dir = recordingsFolder();

	const QString file = QString("%1/%2_%3MHz.wav")
		.arg(dir)
		.arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"))
		.arg(frequency / 100.0, 0, 'f', 1);

	if (!recorder.start(paSource, file))
		return QString();

	recordingClock.start();
	recordingTimer.start();

	return file;
}

// The recorder finalizes the WAV header when it stops
void MediatekRadio::stopRecording()
{
	recorder.stop();
}

// Seconds since the current recording started, 0 when not recording
int MediatekRadio::getRecordingDuration() const
{
	return isRecording() ? recordingClock.elapsed() / 1000 : 0;
}

// The file manager handles file:// urls through url-dispatcher
bool MediatekRadio::openRecordingsFolder()
{
	return QDesktopServices::openUrl(QUrl::fromLocalFile(recordingsFolder()));
}
