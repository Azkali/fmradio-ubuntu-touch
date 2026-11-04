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

#include <QDataStream>
#include <QDebug>

#include "PulseAudio.h"

// Matches the format parecord was invoked with
static const pa_sample_spec kRecordSpec = { PA_SAMPLE_S16LE, 48000, 2 };
static const int kWavHeaderSize = 44;

struct PulseClient::Query {
	pa_threaded_mainloop *loop;
	QString node;
	QStringList names;
	QString activePort;
	bool success = false;
	quint32 index = PA_INVALID_INDEX;
};

static void contextStateCallback(pa_context *, void *userdata)
{
	pa_threaded_mainloop_signal(static_cast<pa_threaded_mainloop *>(userdata), 0);
}

template <typename Info>
static void nodeInfoCallback(pa_context *, const Info *info, int eol, void *userdata)
{
	auto *query = static_cast<PulseClient::Query *>(userdata);

	if (eol) {
		pa_threaded_mainloop_signal(query->loop, 0);
		return;
	}

	const QString name = QString::fromUtf8(info->name);
	query->names << name;
	if (name == query->node && info->active_port)
		query->activePort = QString::fromUtf8(info->active_port->name);
}

static void successCallback(pa_context *, int success, void *userdata)
{
	auto *query = static_cast<PulseClient::Query *>(userdata);

	query->success = success;
	pa_threaded_mainloop_signal(query->loop, 0);
}

static void indexCallback(pa_context *, uint32_t index, void *userdata)
{
	auto *query = static_cast<PulseClient::Query *>(userdata);

	query->index = index;
	pa_threaded_mainloop_signal(query->loop, 0);
}

PulseClient::PulseClient()
{
	loop = pa_threaded_mainloop_new();
	if (!loop)
		return;

	pa_proplist *props = pa_proplist_new();
	pa_proplist_sets(props, PA_PROP_APPLICATION_NAME, "FM Radio");
	pa_proplist_sets(props, PA_PROP_APPLICATION_ID, "fmradio.azkali");
	ctx = pa_context_new_with_proplist(pa_threaded_mainloop_get_api(loop), "FM Radio", props);
	pa_proplist_free(props);
	if (!ctx)
		return;

	pa_context_set_state_callback(ctx, contextStateCallback, loop);

	if (pa_threaded_mainloop_start(loop) < 0) {
		qWarning() << "Failed to start the PulseAudio mainloop";
		return;
	}

	pa_threaded_mainloop_lock(loop);

	if (pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr) >= 0) {
		for (;;) {
			const pa_context_state_t state = pa_context_get_state(ctx);
			if (state == PA_CONTEXT_READY || !PA_CONTEXT_IS_GOOD(state))
				break;
			pa_threaded_mainloop_wait(loop);
		}
	}

	if (pa_context_get_state(ctx) != PA_CONTEXT_READY)
		qWarning() << "Failed to connect to PulseAudio:" << pa_strerror(pa_context_errno(ctx));

	pa_threaded_mainloop_unlock(loop);
}

PulseClient::~PulseClient()
{
	if (loop)
		pa_threaded_mainloop_stop(loop);

	if (ctx) {
		pa_context_set_state_callback(ctx, nullptr, nullptr);
		pa_context_disconnect(ctx);
		pa_context_unref(ctx);
	}

	if (loop)
		pa_threaded_mainloop_free(loop);
}

bool PulseClient::isReady() const
{
	return ctx && pa_context_get_state(ctx) == PA_CONTEXT_READY;
}

// Must be called with the mainloop locked, consumes the operation
bool PulseClient::runLocked(pa_operation *op)
{
	if (!op) {
		qWarning() << "PulseAudio operation failed:" << pa_strerror(pa_context_errno(ctx));
		return false;
	}

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(loop);

	const bool done = pa_operation_get_state(op) == PA_OPERATION_DONE;
	pa_operation_unref(op);

	return done;
}

void PulseClient::listNodes(bool sinks, Query &query)
{
	if (!isReady())
		return;

	query.loop = loop;

	pa_threaded_mainloop_lock(loop);
	if (sinks)
		runLocked(pa_context_get_sink_info_list(ctx, nodeInfoCallback<pa_sink_info>, &query));
	else
		runLocked(pa_context_get_source_info_list(ctx, nodeInfoCallback<pa_source_info>, &query));
	pa_threaded_mainloop_unlock(loop);
}

QStringList PulseClient::sinkNames()
{
	Query query;
	listNodes(true, query);

	return query.names;
}

QStringList PulseClient::sourceNames()
{
	Query query;
	listNodes(false, query);

	return query.names;
}

QString PulseClient::sinkActivePort(const QString &sink)
{
	Query query;
	query.node = sink;
	listNodes(true, query);

	return query.activePort;
}

QString PulseClient::sourceActivePort(const QString &source)
{
	Query query;
	query.node = source;
	listNodes(false, query);

	return query.activePort;
}

bool PulseClient::setPort(bool sink, const QString &node, const QString &port)
{
	if (!isReady() || node.isEmpty() || port.isEmpty())
		return false;

	Query query;
	query.loop = loop;

	const QByteArray nodeName = node.toUtf8();
	const QByteArray portName = port.toUtf8();

	pa_threaded_mainloop_lock(loop);
	if (sink)
		runLocked(pa_context_set_sink_port_by_name(ctx, nodeName.constData(), portName.constData(),
							   successCallback, &query));
	else
		runLocked(pa_context_set_source_port_by_name(ctx, nodeName.constData(), portName.constData(),
							     successCallback, &query));
	pa_threaded_mainloop_unlock(loop);

	if (!query.success)
		qWarning() << "Failed to set port" << port << "on" << node;

	return query.success;
}

bool PulseClient::setSinkPort(const QString &sink, const QString &port)
{
	return setPort(true, sink, port);
}

bool PulseClient::setSourcePort(const QString &source, const QString &port)
{
	return setPort(false, source, port);
}

quint32 PulseClient::loadModule(const QString &name, const QString &args)
{
	if (!isReady())
		return PA_INVALID_INDEX;

	Query query;
	query.loop = loop;

	const QByteArray moduleName = name.toUtf8();
	const QByteArray moduleArgs = args.toUtf8();

	pa_threaded_mainloop_lock(loop);
	runLocked(pa_context_load_module(ctx, moduleName.constData(), moduleArgs.constData(),
					 indexCallback, &query));
	pa_threaded_mainloop_unlock(loop);

	if (query.index == PA_INVALID_INDEX)
		qWarning() << "Failed to load" << name;

	return query.index;
}

bool PulseClient::unloadModule(quint32 index)
{
	if (!isReady() || index == PA_INVALID_INDEX)
		return false;

	Query query;
	query.loop = loop;

	pa_threaded_mainloop_lock(loop);
	runLocked(pa_context_unload_module(ctx, index, successCallback, &query));
	pa_threaded_mainloop_unlock(loop);

	return query.success;
}

static QByteArray wavHeader(quint64 dataBytes)
{
	const quint32 size = static_cast<quint32>(qMin<quint64>(dataBytes, 0xFFFFFFFFu - 36));
	const quint16 channels = kRecordSpec.channels;
	const quint16 bits = 16;
	const quint32 rate = kRecordSpec.rate;

	QByteArray header;
	QDataStream out(&header, QIODevice::WriteOnly);
	out.setByteOrder(QDataStream::LittleEndian);

	out.writeRawData("RIFF", 4);
	out << quint32(36 + size);
	out.writeRawData("WAVE", 4);
	out.writeRawData("fmt ", 4);
	out << quint32(16)                        // fmt chunk size
	    << quint16(1)                         // PCM
	    << channels
	    << rate
	    << quint32(rate * channels * bits / 8) // byte rate
	    << quint16(channels * bits / 8)        // block align
	    << bits;
	out.writeRawData("data", 4);
	out << size;

	return header;
}

PulseRecorder::PulseRecorder(PulseClient &client, QObject *parent)
	: QObject(parent),
	  client(client)
{
}

PulseRecorder::~PulseRecorder()
{
	stop();
}

bool PulseRecorder::isRecording() const
{
	return stream != nullptr;
}

bool PulseRecorder::start(const QString &source, const QString &path)
{
	if (stream || !client.isReady())
		return false;

	file.setFileName(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qWarning() << "Failed to open" << path << file.errorString();
		return false;
	}

	// Placeholder, rewritten with the final sizes when the recording stops
	file.write(wavHeader(0));
	dataBytes = 0;

	const QByteArray sourceName = source.toUtf8();

	pa_threaded_mainloop_lock(client.mainloop());

	stream = pa_stream_new(client.context(), "FM Radio recording", &kRecordSpec, nullptr);
	if (stream) {
		pa_stream_set_state_callback(stream, streamStateCallback, this);
		pa_stream_set_read_callback(stream, streamReadCallback, this);

		if (pa_stream_connect_record(stream, sourceName.constData(), nullptr, PA_STREAM_NOFLAGS) < 0) {
			pa_stream_set_state_callback(stream, nullptr, nullptr);
			pa_stream_set_read_callback(stream, nullptr, nullptr);
			pa_stream_unref(stream);
			stream = nullptr;
		}
	}

	if (!stream)
		qWarning() << "Failed to record from" << source << pa_strerror(pa_context_errno(client.context()));

	pa_threaded_mainloop_unlock(client.mainloop());

	if (!stream) {
		file.close();
		file.remove();
		return false;
	}

	emit recordingChanged();

	return true;
}

void PulseRecorder::stop()
{
	if (!stream)
		return;

	pa_threaded_mainloop_lock(client.mainloop());
	pa_stream_set_state_callback(stream, nullptr, nullptr);
	pa_stream_set_read_callback(stream, nullptr, nullptr);
	pa_stream_disconnect(stream);
	pa_stream_unref(stream);
	stream = nullptr;
	pa_threaded_mainloop_unlock(client.mainloop());

	// Nothing was captured, e.g. the record permission was denied
	if (dataBytes == 0) {
		file.close();
		file.remove();
	} else {
		file.seek(0);
		file.write(wavHeader(dataBytes));
		file.close();
	}

	emit recordingChanged();
}

// Runs on the mainloop thread
void PulseRecorder::streamStateCallback(pa_stream *s, void *userdata)
{
	auto *recorder = static_cast<PulseRecorder *>(userdata);

	if (PA_STREAM_IS_GOOD(pa_stream_get_state(s)))
		return;

	qWarning() << "Recording stream ended:" << pa_strerror(pa_context_errno(pa_stream_get_context(s)));
	QMetaObject::invokeMethod(recorder, [recorder] { recorder->stop(); }, Qt::QueuedConnection);
}

// Runs on the mainloop thread, stop() takes the mainloop lock before touching the file
void PulseRecorder::streamReadCallback(pa_stream *s, size_t, void *userdata)
{
	auto *recorder = static_cast<PulseRecorder *>(userdata);

	while (pa_stream_readable_size(s) > 0) {
		const void *data = nullptr;
		size_t length = 0;

		if (pa_stream_peek(s, &data, &length) < 0 || length == 0)
			return;

		// A null pointer with a length is a hole in the stream
		if (data && recorder->file.write(static_cast<const char *>(data), length) > 0)
			recorder->dataBytes += length;

		pa_stream_drop(s);
	}
}
