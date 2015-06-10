// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/child_trace_message_filter.h"

#include "base/message_loop/message_loop_proxy.h"
#include "base/trace_event/trace_event.h"
#include "components/tracing/tracing_messages.h"
#include "ipc/ipc_channel.h"

using base::trace_event::TraceLog;

namespace tracing {

ChildTraceMessageFilter::ChildTraceMessageFilter(
    base::MessageLoopProxy* ipc_message_loop)
    : sender_(NULL),
      ipc_message_loop_(ipc_message_loop) {}

void ChildTraceMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  sender_ = sender;
  sender_->Send(new TracingHostMsg_ChildSupportsTracing());
}

void ChildTraceMessageFilter::OnFilterRemoved() {
  sender_ = NULL;
}

bool ChildTraceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChildTraceMessageFilter, message)
    IPC_MESSAGE_HANDLER(TracingMsg_BeginTracing, OnBeginTracing)
    IPC_MESSAGE_HANDLER(TracingMsg_EndTracing, OnEndTracing)
    IPC_MESSAGE_HANDLER(TracingMsg_EnableMonitoring, OnEnableMonitoring)
    IPC_MESSAGE_HANDLER(TracingMsg_DisableMonitoring, OnDisableMonitoring)
    IPC_MESSAGE_HANDLER(TracingMsg_CaptureMonitoringSnapshot,
                        OnCaptureMonitoringSnapshot)
    IPC_MESSAGE_HANDLER(TracingMsg_GetTraceLogStatus, OnGetTraceLogStatus)
    IPC_MESSAGE_HANDLER(TracingMsg_SetWatchEvent, OnSetWatchEvent)
    IPC_MESSAGE_HANDLER(TracingMsg_CancelWatchEvent, OnCancelWatchEvent)
    IPC_MESSAGE_HANDLER(TracingMsg_ProcessMemoryDumpRequest,
                        OnProcessMemoryDumpRequest)
    IPC_MESSAGE_HANDLER(TracingMsg_GlobalMemoryDumpResponse,
                        OnGlobalMemoryDumpResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

ChildTraceMessageFilter::~ChildTraceMessageFilter() {}

void ChildTraceMessageFilter::OnBeginTracing(
    const std::string& category_filter_str,
    base::TimeTicks browser_time,
    const std::string& options) {
#if defined(__native_client__)
  // NaCl and system times are offset by a bit, so subtract some time from
  // the captured timestamps. The value might be off by a bit due to messaging
  // latency.
  base::TimeDelta time_offset = base::TimeTicks::NowFromSystemTraceTime() -
      browser_time;
  TraceLog::GetInstance()->SetTimeOffset(time_offset);
#endif
  base::trace_event::TraceOptions trace_options;
  trace_options.SetFromString(options);
  TraceLog::GetInstance()->SetEnabled(
      base::trace_event::CategoryFilter(category_filter_str),
      base::trace_event::TraceLog::RECORDING_MODE,
      trace_options);
}

void ChildTraceMessageFilter::OnEndTracing() {
  TraceLog::GetInstance()->SetDisabled();

  // Flush will generate one or more callbacks to OnTraceDataCollected
  // synchronously or asynchronously. EndTracingAck will be sent in the last
  // OnTraceDataCollected. We are already on the IO thread, so the
  // OnTraceDataCollected calls will not be deferred.
  TraceLog::GetInstance()->Flush(
      base::Bind(&ChildTraceMessageFilter::OnTraceDataCollected, this));
}

void ChildTraceMessageFilter::OnEnableMonitoring(
    const std::string& category_filter_str,
    base::TimeTicks browser_time,
    const std::string& options) {
  base::trace_event::TraceOptions trace_options;
  trace_options.SetFromString(options);
  TraceLog::GetInstance()->SetEnabled(
      base::trace_event::CategoryFilter(category_filter_str),
      base::trace_event::TraceLog::MONITORING_MODE,
      trace_options);
}

void ChildTraceMessageFilter::OnDisableMonitoring() {
  TraceLog::GetInstance()->SetDisabled();
}

void ChildTraceMessageFilter::OnCaptureMonitoringSnapshot() {
  // Flush will generate one or more callbacks to
  // OnMonitoringTraceDataCollected. It's important that the last
  // OnMonitoringTraceDataCollected gets called before
  // CaptureMonitoringSnapshotAck below. We are already on the IO thread,
  // so the OnMonitoringTraceDataCollected calls will not be deferred.
  TraceLog::GetInstance()->FlushButLeaveBufferIntact(
      base::Bind(&ChildTraceMessageFilter::OnMonitoringTraceDataCollected,
                 this));
}

void ChildTraceMessageFilter::OnGetTraceLogStatus() {
  sender_->Send(new TracingHostMsg_TraceLogStatusReply(
      TraceLog::GetInstance()->GetStatus()));
}

void ChildTraceMessageFilter::OnSetWatchEvent(const std::string& category_name,
                                              const std::string& event_name) {
  TraceLog::GetInstance()->SetWatchEvent(
      category_name, event_name,
      base::Bind(&ChildTraceMessageFilter::OnWatchEventMatched, this));
}

void ChildTraceMessageFilter::OnCancelWatchEvent() {
  TraceLog::GetInstance()->CancelWatchEvent();
}

void ChildTraceMessageFilter::OnWatchEventMatched() {
  if (!ipc_message_loop_->BelongsToCurrentThread()) {
    ipc_message_loop_->PostTask(FROM_HERE,
        base::Bind(&ChildTraceMessageFilter::OnWatchEventMatched, this));
    return;
  }
  sender_->Send(new TracingHostMsg_WatchEventMatched);
}

void ChildTraceMessageFilter::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr,
    bool has_more_events) {
  if (!ipc_message_loop_->BelongsToCurrentThread()) {
    ipc_message_loop_->PostTask(FROM_HERE,
        base::Bind(&ChildTraceMessageFilter::OnTraceDataCollected, this,
                   events_str_ptr, has_more_events));
    return;
  }
  if (events_str_ptr->data().size()) {
    sender_->Send(new TracingHostMsg_TraceDataCollected(
        events_str_ptr->data()));
  }
  if (!has_more_events) {
    std::vector<std::string> category_groups;
    TraceLog::GetInstance()->GetKnownCategoryGroups(&category_groups);
    sender_->Send(new TracingHostMsg_EndTracingAck(category_groups));
  }
}

void ChildTraceMessageFilter::OnMonitoringTraceDataCollected(
     const scoped_refptr<base::RefCountedString>& events_str_ptr,
     bool has_more_events) {
  if (!ipc_message_loop_->BelongsToCurrentThread()) {
    ipc_message_loop_->PostTask(FROM_HERE,
        base::Bind(&ChildTraceMessageFilter::
                   OnMonitoringTraceDataCollected,
                   this,
                   events_str_ptr,
                   has_more_events));
    return;
  }
  sender_->Send(new TracingHostMsg_MonitoringTraceDataCollected(
      events_str_ptr->data()));

  if (!has_more_events)
    sender_->Send(new TracingHostMsg_CaptureMonitoringSnapshotAck());
}

// Sent by the Browser's MemoryDumpManager when coordinating a global dump.
void ChildTraceMessageFilter::OnProcessMemoryDumpRequest(
    const base::trace_event::MemoryDumpRequestArgs& args) {
  // TODO(primiano): create local dump and send a response back to the browser.
  NOTIMPLEMENTED();
}

// Initiates a dump request, asking the Browser's MemoryDumpManager to
// coordinate a global memory dump. The Browser's MDM will answer back with a
// MemoryDumpResponse when all the child processes (including this one) have
// dumped, or with a NACK (|success| == false) if the dump failed (e.g., due to
// a collision with a concurrent request from another child process).
void ChildTraceMessageFilter::SendGlobalMemoryDumpRequest(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::trace_event::MemoryDumpCallback& callback) {
  // TODO(primiano): implement the logic to send the request to the browser
  // process and keep track of that.
  NOTIMPLEMENTED();
}

// Sent by the Browser's MemoryDumpManager in response of a dump request
// initiated by this child process.
void ChildTraceMessageFilter::OnGlobalMemoryDumpResponse(uint64 dump_guid,
                                                         bool success) {
  // TODO(primiano): implement the logic to handle the global response from
  // the browser and clear up the bookkeeping.
  NOTIMPLEMENTED();
}

}  // namespace tracing
