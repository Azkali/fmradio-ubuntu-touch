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

#ifndef PULSEAUDIO_H
#define PULSEAUDIO_H

#include <QFile>
#include <QObject>
#include <QString>
#include <QStringList>

#include <pulse/pulseaudio.h>

// Synchronous wrapper around the libpulse async API, replacing pactl.
// Confined apps may talk to the PulseAudio native socket but may not
// execute /usr/bin/pactl.
class PulseClient {
public:
	PulseClient();
	~PulseClient();

	PulseClient(const PulseClient &) = delete;
	PulseClient &operator=(const PulseClient &) = delete;

	bool isReady() const;

	QStringList sinkNames();
	QStringList sourceNames();
	QString sinkActivePort(const QString &sink);
	QString sourceActivePort(const QString &source);
	bool setSinkPort(const QString &sink, const QString &port);
	bool setSourcePort(const QString &source, const QString &port);

	// Returns PA_INVALID_INDEX on failure
	quint32 loadModule(const QString &name, const QString &args);
	bool unloadModule(quint32 index);

	pa_threaded_mainloop *mainloop() const { return loop; }
	pa_context *context() const { return ctx; }

	// Shared between a blocking call and the mainloop thread callbacks
	struct Query;

private:

	void listNodes(bool sinks, Query &query);
	bool setPort(bool sink, const QString &node, const QString &port);
	bool runLocked(pa_operation *op);

	pa_threaded_mainloop *loop = nullptr;
	pa_context *ctx = nullptr;
};

// Records a source to a 16 bit 48 kHz stereo WAV file, replacing parecord.
// The first recording goes through the trust-store "wants to record audio"
// prompt when the app is confined.
class PulseRecorder : public QObject {
	Q_OBJECT

public:
	explicit PulseRecorder(PulseClient &client, QObject *parent = nullptr);
	~PulseRecorder();

	bool start(const QString &source, const QString &path);
	void stop();
	bool isRecording() const;

signals:
	void recordingChanged();

private:
	static void streamStateCallback(pa_stream *s, void *userdata);
	static void streamReadCallback(pa_stream *s, size_t length, void *userdata);

	PulseClient &client;
	pa_stream *stream = nullptr;
	QFile file;
	quint64 dataBytes = 0;
};

#endif
