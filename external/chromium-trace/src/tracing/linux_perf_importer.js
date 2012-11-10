// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Imports text files in the Linux event trace format into the
 * timeline model. This format is output both by sched_trace and by Linux's perf
 * tool.
 *
 * This importer assumes the events arrive as a string. The unit tests provide
 * examples of the trace format.
 *
 * Linux scheduler traces use a definition for 'pid' that is different than
 * tracing uses. Whereas tracing uses pid to identify a specific process, a pid
 * in a linux trace refers to a specific thread within a process. Within this
 * file, we the definition used in Linux traces, as it improves the importing
 * code's readability.
 */
cr.define('tracing', function() {
  /**
   * Represents the scheduling state for a single thread.
   * @constructor
   */
  function CpuState(cpu) {
    this.cpu = cpu;
  }

  CpuState.prototype = {
    __proto__: Object.prototype,

    /**
     * Switches the active pid on this Cpu. If necessary, add a TimelineSlice
     * to the cpu representing the time spent on that Cpu since the last call to
     * switchRunningLinuxPid.
     */
    switchRunningLinuxPid: function(importer, prevState, ts, pid, comm, prio) {
      // Generate a slice if the last active pid was not the idle task
      if (this.lastActivePid !== undefined && this.lastActivePid != 0) {
        var duration = ts - this.lastActiveTs;
        var thread = importer.threadsByLinuxPid[this.lastActivePid];
        if (thread)
          name = thread.userFriendlyName;
        else
          name = this.lastActiveComm;

        var slice = new tracing.TimelineSlice(name,
                                              tracing.getStringColorId(name),
                                              this.lastActiveTs,
                                              {
                                                comm: this.lastActiveComm,
                                                tid: this.lastActivePid,
                                                prio: this.lastActivePrio,
                                                stateWhenDescheduled: prevState
                                              },
                                              duration);
        this.cpu.slices.push(slice);
      }

      this.lastActiveTs = ts;
      this.lastActivePid = pid;
      this.lastActiveComm = comm;
      this.lastActivePrio = prio;
    }
  };

  function ThreadState(tid) {
    this.openSlices = [];
  }

  /**
   * Imports linux perf events into a specified model.
   * @constructor
   */
  function LinuxPerfImporter(model, events, isAdditionalImport) {
    this.isAdditionalImport_ = isAdditionalImport;
    this.model_ = model;
    this.events_ = events;
    this.clockSyncRecords_ = [];
    this.cpuStates_ = {};
    this.kernelThreadStates_ = {};
    this.buildMapFromLinuxPidsToTimelineThreads();
    this.lineNumber = -1;

    // To allow simple indexing of threads, we store all the threads by their
    // kernel KPID. The KPID is a unique key for a thread in the trace.
    this.threadStateByKPID_ = {};
  }

  TestExports = {};

  // Matches the generic trace record:
  //          <idle>-0     [001] .... 1.23: sched_switch
  var lineRE = /^\s*(.+?)\s+\[(\d+)\]\s*([d.][N.][sh.][\d.])?\s*(\d+\.\d+):\s+(\S+):\s(.*)$/;
  TestExports.lineRE = lineRE;

  // Matches the trace_event_clock_sync record
  //  0: trace_event_clock_sync: parent_ts=19581477508
  var traceEventClockSyncRE = /trace_event_clock_sync: parent_ts=(\d+\.?\d*)/;
  TestExports.traceEventClockSyncRE = traceEventClockSyncRE;

  /**
   * Guesses whether the provided events is a Linux perf string.
   * Looks for the magic string "# tracer" at the start of the file,
   * or the typical task-pid-cpu-timestamp-function sequence of a typical
   * trace's body.
   *
   * @return {boolean} True when events is a linux perf array.
   */
  LinuxPerfImporter.canImport = function(events) {
    if (!(typeof(events) === 'string' || events instanceof String))
      return false;

    if (/^# tracer:/.exec(events))
      return true;

    var m = /^(.+)\n/.exec(events);
    if (m)
      events = m[1];
    if (lineRE.exec(events))
      return true;

    return false;
  };

  LinuxPerfImporter.prototype = {
    __proto__: Object.prototype,

    /**
     * Precomputes a lookup table from linux pids back to existing
     * TimelineThreads. This is used during importing to add information to each
     * timeline thread about whether it was running, descheduled, sleeping, et
     * cetera.
     */
    buildMapFromLinuxPidsToTimelineThreads: function() {
      this.threadsByLinuxPid = {};
      this.model_.getAllThreads().forEach(
          function(thread) {
            this.threadsByLinuxPid[thread.tid] = thread;
          }.bind(this));
    },

    /**
     * @return {CpuState} A CpuState corresponding to the given cpuNumber.
     */
    getOrCreateCpuState: function(cpuNumber) {
      if (!this.cpuStates_[cpuNumber]) {
        var cpu = this.model_.getOrCreateCpu(cpuNumber);
        this.cpuStates_[cpuNumber] = new CpuState(cpu);
      }
      return this.cpuStates_[cpuNumber];
    },

    /**
     * @return {number} The pid extracted from the kernel thread name.
     */
    parsePid: function(kernelThreadName) {
        var pid = /.+-(\d+)/.exec(kernelThreadName)[1];
        pid = parseInt(pid);
        return pid;
    },

    /**
     * @return {number} The string portion of the thread extracted from the
     * kernel thread name.
     */
    parseThreadName: function(kernelThreadName) {
        return /(.+)-\d+/.exec(kernelThreadName)[1];
    },

    /**
     * @return {TimelinThread} A thread corresponding to the kernelThreadName.
     */
    getOrCreateKernelThread: function(kernelThreadName, opt_pid, opt_tid) {
      if (!this.kernelThreadStates_[kernelThreadName]) {
        var pid = opt_pid;
        if (pid == undefined) {
          pid = this.parsePid(kernelThreadName);
        }
        var tid = opt_tid;
        if (tid == undefined)
          tid = pid;

        var thread = this.model_.getOrCreateProcess(pid).getOrCreateThread(tid);
        thread.name = kernelThreadName;
        this.kernelThreadStates_[kernelThreadName] = {
          pid: pid,
          thread: thread,
          openSlice: undefined,
          openSliceTS: undefined,
          asyncSlices: {}
        };
        this.threadsByLinuxPid[pid] = thread;
      }
      return this.kernelThreadStates_[kernelThreadName];
    },

    /**
     * Imports the data in this.events_ into model_.
     */
    importEvents: function() {
      this.importCpuData();
      if (!this.alignClocks())
        return;
      this.buildPerThreadCpuSlicesFromCpuState();
    },

    /**
     * Called by the TimelineModel after all other importers have imported their
     * events.
     */
    finalizeImport: function() {
    },

    /**
     * Builds the cpuSlices array on each thread based on our knowledge of what
     * each Cpu is doing.  This is done only for TimelineThreads that are
     * already in the model, on the assumption that not having any traced data
     * on a thread means that it is not of interest to the user.
     */
    buildPerThreadCpuSlicesFromCpuState: function() {
      // Push the cpu slices to the threads that they run on.
      for (var cpuNumber in this.cpuStates_) {
        var cpuState = this.cpuStates_[cpuNumber];
        var cpu = cpuState.cpu;

        for (var i = 0; i < cpu.slices.length; i++) {
          var slice = cpu.slices[i];

          var thread = this.threadsByLinuxPid[slice.args.tid];
          if (!thread)
            continue;
          if (!thread.tempCpuSlices)
            thread.tempCpuSlices = [];

          // Because Chrome's Array.sort is not a stable sort, we need to keep
          // the slice index around to keep slices with identical start times in
          // the proper order when sorting them.
          slice.index = i;

          thread.tempCpuSlices.push(slice);
        }
      }

      // Create slices for when the thread is not running.
      var runningId = tracing.getColorIdByName('running');
      var runnableId = tracing.getColorIdByName('runnable');
      var sleepingId = tracing.getColorIdByName('sleeping');
      var ioWaitId = tracing.getColorIdByName('iowait');
      this.model_.getAllThreads().forEach(function(thread) {
        if (!thread.tempCpuSlices)
          return;
        var origSlices = thread.tempCpuSlices;
        delete thread.tempCpuSlices;

        origSlices.sort(function(x, y) {
          var delta = x.start - y.start;
          if (delta == 0) {
            // Break ties using the original slice ordering.
            return x.index - y.index;
          } else {
            return delta;
          }
        });

        // Walk the slice list and put slices between each original slice
        // to show when the thread isn't running
        var slices = [];
        if (origSlices.length) {
          var slice = origSlices[0];
          slices.push(new tracing.TimelineSlice('Running', runningId,
              slice.start, {}, slice.duration));
        }
        for (var i = 1; i < origSlices.length; i++) {
          var prevSlice = origSlices[i - 1];
          var nextSlice = origSlices[i];
          var midDuration = nextSlice.start - prevSlice.end;
          if (prevSlice.args.stateWhenDescheduled == 'S') {
            slices.push(new tracing.TimelineSlice('Sleeping', sleepingId,
                prevSlice.end, {}, midDuration));
          } else if (prevSlice.args.stateWhenDescheduled == 'R' ||
                     prevSlice.args.stateWhenDescheduled == 'R+') {
            slices.push(new tracing.TimelineSlice('Runnable', runnableId,
                prevSlice.end, {}, midDuration));
          } else if (prevSlice.args.stateWhenDescheduled == 'D') {
            slices.push(new tracing.TimelineSlice(
              'Uninterruptible Sleep', ioWaitId,
              prevSlice.end, {}, midDuration));
          } else if (prevSlice.args.stateWhenDescheduled == 'T') {
            slices.push(new tracing.TimelineSlice('__TASK_STOPPED', ioWaitId,
                prevSlice.end, {}, midDuration));
          } else if (prevSlice.args.stateWhenDescheduled == 't') {
            slices.push(new tracing.TimelineSlice('debug', ioWaitId,
                prevSlice.end, {}, midDuration));
          } else if (prevSlice.args.stateWhenDescheduled == 'Z') {
            slices.push(new tracing.TimelineSlice('Zombie', ioWaitId,
                prevSlice.end, {}, midDuration));
          } else if (prevSlice.args.stateWhenDescheduled == 'X') {
            slices.push(new tracing.TimelineSlice('Exit Dead', ioWaitId,
                prevSlice.end, {}, midDuration));
          } else if (prevSlice.args.stateWhenDescheduled == 'x') {
            slices.push(new tracing.TimelineSlice('Task Dead', ioWaitId,
                prevSlice.end, {}, midDuration));
          } else if (prevSlice.args.stateWhenDescheduled == 'W') {
            slices.push(new tracing.TimelineSlice('WakeKill', ioWaitId,
                prevSlice.end, {}, midDuration));
          } else if (prevSlice.args.stateWhenDescheduled == 'D|W') {
            slices.push(new tracing.TimelineSlice(
              'Uninterruptible Sleep | WakeKill', ioWaitId,
              prevSlice.end, {}, midDuration));
          } else {
            throw 'Unrecognized state: ' + prevSlice.args.stateWhenDescheduled;
          }

          slices.push(new tracing.TimelineSlice('Running', runningId,
              nextSlice.start, {}, nextSlice.duration));
        }
        thread.cpuSlices = slices;
      });
    },

    /**
     * Walks the slices stored on this.cpuStates_ and adjusts their timestamps
     * based on any alignment metadata we discovered.
     */
    alignClocks: function() {
      if (this.clockSyncRecords_.length == 0) {
        // If this is an additional import, and no clock syncing records were
        // found, then abort the import. Otherwise, just skip clock alignment.
        if (!this.isAdditionalImport_)
          return;

        // Remove the newly imported CPU slices from the model.
        this.abortImport();
        return false;
      }

      // Shift all the slice times based on the sync record.
      var sync = this.clockSyncRecords_[0];
      // NB: parentTS of zero denotes no times-shift; this is
      // used when user and kernel event clocks are identical.
      if (sync.parentTS == 0 || sync.parentTS == sync.perfTS)
        return true;
      var timeShift = sync.parentTS - sync.perfTS;
      for (var cpuNumber in this.cpuStates_) {
        var cpuState = this.cpuStates_[cpuNumber];
        var cpu = cpuState.cpu;

        for (var i = 0; i < cpu.slices.length; i++) {
          var slice = cpu.slices[i];
          slice.start = slice.start + timeShift;
          slice.duration = slice.duration;
        }

        for (var counterName in cpu.counters) {
          var counter = cpu.counters[counterName];
          for (var sI = 0; sI < counter.timestamps.length; sI++)
            counter.timestamps[sI] = (counter.timestamps[sI] + timeShift);
        }
      }
      for (var kernelThreadName in this.kernelThreadStates_) {
        var kthread = this.kernelThreadStates_[kernelThreadName];
        var thread = kthread.thread;
        for (var i = 0; i < thread.subRows[0].length; i++) {
          thread.subRows[0][i].start += timeShift;
        }
      }
      return true;
    },

    /**
     * Removes any data that has been added to the model because of an error
     * detected during the import.
     */
    abortImport: function() {
      if (this.pushedEventsToThreads)
        throw 'Cannot abort, have alrady pushedCpuDataToThreads.';

      for (var cpuNumber in this.cpuStates_)
        delete this.model_.cpus[cpuNumber];
      for (var kernelThreadName in this.kernelThreadStates_) {
        var kthread = this.kernelThreadStates_[kernelThreadName];
        var thread = kthread.thread;
        var process = thread.parent;
        delete process.threads[thread.tid];
        delete this.model_.processes[process.pid];
      }
      this.model_.importErrors.push(
          'Cannot import kernel trace without a clock sync.');
    },

    /**
     * Records the fact that a pid has become runnable. This data will
     * eventually get used to derive each thread's cpuSlices array.
     */
    markPidRunnable: function(ts, pid, comm, prio) {
      // TODO(nduca): implement this functionality.
    },

    importError: function(message) {
      this.model_.importErrors.push('Line ' + (this.lineNumber + 1) +
          ': ' + message);
    },

    malformedEvent: function(eventName) {
      this.importError('Malformed ' + eventName + ' event');
    },

    /**
     * Helper to open a kernel thread slice.
     */
    openSlice: function(kthread, name, ts) {
      kthread.openSliceTS = ts;
      kthread.openSlice = name;
    },

    /**
     * Helper to close a kernel thread slice.
     */
    closeSlice: function(kthread, ts, data) {
      if (kthread.openSlice) {
        var slice = new tracing.TimelineSlice(kthread.openSlice,
            tracing.getStringColorId(kthread.openSlice),
            kthread.openSliceTS,
            data,
            ts - kthread.openSliceTS);
        kthread.thread.subRows[0].push(slice);
        kthread.openSlice = undefined;
      }
    },

    /**
     * Helper to open an async slice.
     */
    openAsyncSlice: function(kthread, key, ts, name) {
      var slice = new tracing.TimelineAsyncSlice(name,
          tracing.getStringColorId(name), ts);
      slice.startThread = kthread.thread;
      kthread.asyncSlices[key] = slice;
    },

    /**
     * Helper to close an async slice.
     */
    closeAsyncSlice: function(kthread, key, ts, data) {
      var slice = kthread.asyncSlices[key];
      if (slice) {
        slice.duration = ts - slice.start;
        slice.args = data;
        slice.endThread = kthread.thread;
        slice.subSlices = [ new tracing.TimelineSlice(slice.title,
           slice.colorId, slice.start, slice.args, slice.duration) ];
        kthread.thread.asyncSlices.push(slice);
        delete kthread.asyncSlices[key];
      }
    },

    /**
     * Helper to get a ThreadState for a given taskId.
     */
    getThreadState: function(taskId) {
      var kpid = this.parsePid(taskId);
      return this.threadStateByKPID_[kpid];
    },

    /**
     * Helper to get or create a ThreadState for a given taskId.
     */
    getOrCreateThreadState: function(taskId, pid) {
      var kpid = this.parsePid(taskId);
      var state = this.threadStateByKPID_[kpid];
      if (!state) {
        state = new ThreadState();
        state.threadName = this.parseThreadName(taskId);
        state.tid = kpid;
        state.pid = pid;
        state.thread = this.model_.getOrCreateProcess(pid).
            getOrCreateThread(kpid);
        this.threadsByLinuxPid[kpid] = state.thread;
        if (!state.thread.name) {
          state.thread.name = state.threadName;
        }
        this.threadStateByKPID_[kpid] = state;
      }
      return state;
    },

    /**
     * Helper to process a 'begin' event (e.g. initiate a slice).
     * @param {string} name The trace event name.
     * @param {number} ts The trace event begin timestamp.
     */
    processBegin: function(taskId, name, ts, pid) {
      var state = this.getOrCreateThreadState(taskId, pid);
      var colorId = tracing.getStringColorId(name);
      var slice = new tracing.TimelineThreadSlice(name, colorId, ts, null);
      state.openSlices.push(slice);
    },

    /**
     * Helper to process an 'end' event (e.g. close a slice).
     * @param {number} ts The trace event begin timestamp.
     */
    processEnd: function(taskId, ts) {
      var state = this.getThreadState(taskId);
      if (!state || state.openSlices.length == 0) {
        // Ignore E events that are unmatched.
        return;
      }
      var slice = state.openSlices.pop();
      slice.duration = ts - slice.start;

      // Store the slice on the correct subrow.
      var subRowIndex = state.openSlices.length;
      state.thread.getSubrow(subRowIndex).push(slice);

      // Add the slice to the subSlices array of its parent.
      if (state.openSlices.length) {
        var parentSlice = state.openSlices[state.openSlices.length - 1];
        parentSlice.subSlices.push(slice);
      }
    },

    /**
     * Helper function that closes any open slices. This happens when a trace
     * ends before an 'E' phase event can get posted. When that happens, this
     * closes the slice at the highest timestamp we recorded and sets the
     * didNotFinish flag to true.
     */
    autoCloseOpenSlices: function() {
      // We need to know the model bounds in order to assign an end-time to
      // the open slices.
      this.model_.updateBounds();

      // The model's max value in the trace is wrong at this point if there are
      // un-closed events. To close those events, we need the true global max
      // value. To compute this, build a list of timestamps that weren't
      // included in the max calculation, then compute the real maximum based
      // on that.
      var openTimestamps = [];
      for (var kpid in this.threadStateByKPID_) {
        var state = this.threadStateByKPID_[kpid];
        for (var i = 0; i < state.openSlices.length; i++) {
          var slice = state.openSlices[i];
          openTimestamps.push(slice.start);
          for (var s = 0; s < slice.subSlices.length; s++) {
            var subSlice = slice.subSlices[s];
            openTimestamps.push(subSlice.start);
            if (subSlice.duration)
              openTimestamps.push(subSlice.end);
          }
        }
      }

      // Figure out the maximum value of model.maxTimestamp and
      // Math.max(openTimestamps). Made complicated by the fact that the model
      // timestamps might be undefined.
      var realMaxTimestamp;
      if (this.model_.maxTimestamp) {
        realMaxTimestamp = Math.max(this.model_.maxTimestamp,
                                    Math.max.apply(Math, openTimestamps));
      } else {
        realMaxTimestamp = Math.max.apply(Math, openTimestamps);
      }

      // Automatically close any slices are still open. These occur in a number
      // of reasonable situations, e.g. deadlock. This pass ensures the open
      // slices make it into the final model.
      for (var kpid in this.threadStateByKPID_) {
        var state = this.threadStateByKPID_[kpid];
        while (state.openSlices.length > 0) {
          var slice = state.openSlices.pop();
          slice.duration = realMaxTimestamp - slice.start;
          slice.didNotFinish = true;

          // Store the slice on the correct subrow.
          var subRowIndex = state.openSlices.length;
          state.thread.getSubrow(subRowIndex).push(slice);

          // Add the slice to the subSlices array of its parent.
          if (state.openSlices.length) {
            var parentSlice = state.openSlices[state.openSlices.length - 1];
            parentSlice.subSlices.push(slice);
          }
        }
      }
    },

    /**
     * Helper that creates and adds samples to a TimelineCounter object based on
     * 'C' phase events.
     */
    processCounter: function(name, ts, value, pid) {
      var ctr = this.model_.getOrCreateProcess(pid)
          .getOrCreateCounter('', name);

      // Initialize the counter's series fields if needed.
      //
      if (ctr.numSeries == 0) {
        ctr.seriesNames.push('state');
        ctr.seriesColors.push(
            tracing.getStringColorId(ctr.name + '.' + 'state'));
      }

      // Add the sample values.
      ctr.timestamps.push(ts);
      ctr.samples.push(value);
    },

    /**
     * Walks the this.events_ structure and creates TimelineCpu objects.
     */
    importCpuData: function() {
      this.lines_ = this.events_.split('\n');

      for (this.lineNumber = 0; this.lineNumber < this.lines_.length;
          ++this.lineNumber) {
        var line = this.lines_[this.lineNumber];
        if (/^#/.exec(line) || line.length == 0)
          continue;
        var eventBase = lineRE.exec(line);
        if (!eventBase) {
          this.importError('Unrecognized line: ' + line);
          continue;
        }

        var taskId = eventBase[1];
        var cpuNum = eventBase[2];
        var taskInfo = eventBase[3];
        var timestamp = eventBase[4];
        var eventName = eventBase[5];
        var eventInfo = eventBase[6];

        var eventDefinition = this.eventDefinitions[eventName];
        if (eventDefinition) {
          // Parse the event info.
          var event;
          if (eventDefinition.format) {
            event = eventDefinition.format.exec(eventInfo);
            if (!event) {
              this.malformedEvent(eventName);
              continue;
            }
          } else {
            event = {};
          }

          // Add the basic event properties.
          event.timestamp = parseFloat(timestamp) * 1000;
          event.name = eventName;
          event.info = eventInfo;
          event.taskId = taskId;
          event.cpuState = this.getOrCreateCpuState(parseInt(cpuNum));

          // Invoke the handler.
          if (eventDefinition.handler) {
            eventDefinition.handler(this, event);
          }
        } else {
          console.log('unknown event ' + eventName);
        }
      }
    },

    /**
     * Table of supported events represented as an associative array indexed by event name.
     * Each event definition has the following properties:
     *
     *   format:   A regular expression to parse the event info.
     *             If omitted, the event information is not parsed but the other basic
     *             properties are still provided to the event handler.
     *   handler:  A handler function to invoke to handle the event.
     *             If omitted, the event is parsed but not handled.
     *
     * The event object passed as a parameter to the handler has the matched groups from
     * from the regular expression and in addition has the following properties:
     *
     *   timestamp: The uptime in milliseconds.
     *   name:      The event name.
     *   info:      The unparsed event info.
     *   taskId:    The task ID.
     *   cpuState:  The CPU state object for the CPU associated with the event.
     */
    eventDefinitions: {
      'sched_switch': {
        format: new RegExp(
            'prev_comm=(.+) prev_pid=(\\d+) prev_prio=(\\d+) prev_state=(\\S+) ==> ' +
            'next_comm=(.+) next_pid=(\\d+) next_prio=(\\d+)'),
        handler: function(importer, event) {
          var prevState = event[4];
          var nextComm = event[5];
          var nextPid = parseInt(event[6]);
          var nextPrio = parseInt(event[7]);
          event.cpuState.switchRunningLinuxPid(
              importer, prevState, event.timestamp, nextPid, nextComm, nextPrio);
        }
      },

      'sched_wakeup': {
        format: /comm=(.+) pid=(\d+) prio=(\d+) success=(\d+) target_cpu=(\d+)/,
        handler: function(importer, event) {
          var comm = event[1];
          var pid = parseInt(event[2]);
          var prio = parseInt(event[3]);
          importer.markPidRunnable(event.timestamp, pid, comm, prio);
        }
      },

      'power_start': { // NB: old-style power event, deprecated
        format: /type=(\d+) state=(\d) cpu_id=(\d)+/,
        handler: function(importer, event) {
          var targetCpuNumber = parseInt(event[3]);
          var targetCpu = importer.getOrCreateCpuState(targetCpuNumber);
          var powerCounter;
          if (event[1] == '1') {
            powerCounter = targetCpu.cpu.getOrCreateCounter('', 'C-State');
          } else {
            importer.importError('Don\'t understand power_start events of ' +
                'type ' + event[1]);
            return;
          }
          if (powerCounter.numSeries == 0) {
            powerCounter.seriesNames.push('state');
            powerCounter.seriesColors.push(
                tracing.getStringColorId(powerCounter.name + '.' + 'state'));
          }
          var powerState = parseInt(event[2]);
          powerCounter.timestamps.push(event.timestamp);
          powerCounter.samples.push(powerState);
        }
      },

      'power_frequency': { // NB: old-style power event, deprecated
        format: /type=(\d+) state=(\d+) cpu_id=(\d)+/,
        handler: function(importer, event) {
          var targetCpuNumber = parseInt(event[3]);
          var targetCpu = importer.getOrCreateCpuState(targetCpuNumber);
          var powerCounter = targetCpu.cpu.getOrCreateCounter('', 'Power Frequency');
          if (powerCounter.numSeries == 0) {
            powerCounter.seriesNames.push('state');
            powerCounter.seriesColors.push(
                tracing.getStringColorId(powerCounter.name + '.' + 'state'));
          }
          var powerState = parseInt(event[2]);
          powerCounter.timestamps.push(event.timestamp);
          powerCounter.samples.push(powerState);
        }
      },

      'cpu_frequency': {
        format: /state=(\d+) cpu_id=(\d)+/,
        handler: function(importer, event) {
          var targetCpuNumber = parseInt(event[2]);
          var targetCpu = importer.getOrCreateCpuState(targetCpuNumber);
          var powerCounter = targetCpu.cpu.getOrCreateCounter('', 'Clock Frequency');
          if (powerCounter.numSeries == 0) {
            powerCounter.seriesNames.push('state');
            powerCounter.seriesColors.push(
                tracing.getStringColorId(powerCounter.name + '.' + 'state'));
          }
          var powerState = parseInt(event[1]);
          powerCounter.timestamps.push(event.timestamp);
          powerCounter.samples.push(powerState);
        }
      },

      'cpu_idle': {
        format: /state=(\d+) cpu_id=(\d)+/,
        handler: function(importer, event) {
          var targetCpuNumber = parseInt(event[2]);
          var targetCpu = importer.getOrCreateCpuState(targetCpuNumber);
          var powerCounter = targetCpu.cpu.getOrCreateCounter('', 'C-State');
          if (powerCounter.numSeries == 0) {
            powerCounter.seriesNames.push('state');
            powerCounter.seriesColors.push(
                tracing.getStringColorId(powerCounter.name));
          }
          var powerState = parseInt(event[1]);
          // NB: 4294967295/-1 means an exit from the current state
          if (powerState != 4294967295)
            powerCounter.samples.push(powerState);
          else
            powerCounter.samples.push(0);
          powerCounter.timestamps.push(event.timestamp);
        }
      },

      'workqueue_execute_start': {
        // workqueue_execute_start: work struct c7a8a89c: function MISRWrapper
        format: /work struct (.+): function (\S+)/,
        handler: function(importer, event) {
          var kthread = importer.getOrCreateKernelThread(event.taskId);
          importer.openSlice(kthread, event[2], event.timestamp);
        }
      },

      'workqueue_execute_end': {
        // workqueue_execute_end: work struct c7a8a89c
        format: /work struct (.+)/,
        handler: function(importer, event) {
          var kthread = importer.getOrCreateKernelThread(event.taskId);
          importer.closeSlice(kthread, event.timestamp, {});
        }
      },

      'workqueue_queue_work': {
        // ignored for now
      },

      'workqueue_activate_work': {
        // ignored for now
      },

      'ext4_sync_file_enter': {
        // ext4_sync_file_enter: dev 179,9 ino 114914 parent 114912 datasync 1
        format: /dev (\d+,\d+) ino (\d+) parent (\d+) datasync (\d+)/,
        handler: function(importer, event) {
          var kthread = importer.getOrCreateKernelThread('ext4:' + event.taskId);
          var device = event[1];
          var inode = event[2];
          var datasync = event[4] == 1;
          var key = device + '-' + inode;
          importer.openAsyncSlice(kthread, key, event.timestamp,
              datasync ? 'fdatasync' : 'fsync');
        }
      },

      'ext4_sync_file_exit': {
        // ext4_sync_file_exit: dev 179,9 ino 114912 ret 0
        format: /dev (\d+,\d+) ino (\d+) ret (\d+)/,
        handler: function(importer, event) {
          var kthread = importer.getOrCreateKernelThread('ext4:' + event.taskId);
          var device = event[1];
          var inode = event[2];
          var error = parseInt(event[3]);
          var key = device + '-' + inode;
          importer.closeAsyncSlice(kthread, key, event.timestamp, {
                device: device,
                inode: inode,
                error: error
              });
        }
      },

      'block_rq_issue': {
        // block_rq_issue: 179,0 WS 0 () 9182248 + 8 [mmcqd/0]
        format: /(\d+,\d+) (F)?([DWRN])(F)?(A)?(S)?(M)? \d+ \(.*\) (\d+) \+ (\d+) \[.*\]/,
        handler: function(importer, event) {
          var action;
          switch (event[3]) {
            case 'D':
              action = 'discard';
              break;
            case 'W':
              action = 'write';
              break;
            case 'R':
              action = 'read';
              break;
            case 'N':
              action = 'none';
              break;
            default:
              action = 'unknown';
              break;
          }

          if (event[2]) {
            action += ' flush';
          }
          if (event[4] == 'F') {
            action += ' fua';
          }
          if (event[5] == 'A') {
            action += ' ahead';
          }
          if (event[6] == 'S') {
            action += ' sync';
          }
          if (event[7] == 'M') {
            action += ' meta';
          }
          var device = event[1]
          var sector = parseInt(event[8])
          var numSectors = parseInt(event[9])
          var kthread = importer.getOrCreateKernelThread('block:' + event.taskId);
          var key = device + '-' + sector + '-' + numSectors;
          importer.openAsyncSlice(kthread, key, event.timestamp, action);
        }
      },

      'block_rq_complete': {
        // block_rq_complete: 179,0 WS () 9182248 + 8 [0]
        format: /(\d+,\d+) (F)?([DWRN])(F)?(A)?(S)?(M)? \(.*\) (\d+) \+ (\d+) \[(.*)\]/,
        handler: function(importer, event) {
          var device = event[1]
          var sector = parseInt(event[8])
          var numSectors = parseInt(event[9])
          var error = parseInt(event[10])
          var kthread = importer.getOrCreateKernelThread('block:' + event.taskId);
          var key = device + '-' + sector + '-' + numSectors;
          importer.closeAsyncSlice(kthread, key, event.timestamp, {
              device: device,
              sector: sector,
              numSectors: numSectors,
              error: error
          });
        }
      },

      'i915_gem_object_pwrite': {
        format: /obj=(.+), offset=(\d+), len=(\d+)/,
        handler: function(importer, event) {
          var obj = event[1];
          var offset = parseInt(event[2]);
          var len = parseInt(event[3]);
          var kthread = importer.getOrCreateKernelThread('i915_gem', 0, 1);
          importer.openSlice(kthread, 'pwrite:' + obj, event.timestamp);
          importer.closeSlice(kthread, event.timestamp, {
                obj: obj,
                offset: offset,
                len: len
              });
        }
      },

      'i915_flip_request': {
        format: /plane=(\d+), obj=(.+)/,
        handler: function(importer, event) {
          var plane = parseInt(event[1]);
          var obj = event[2];
          // use i915_obj_plane?
          var kthread = importer.getOrCreateKernelThread('i915_flip', 0, 2);
          importer.openSlice(kthread, 'flip:' + obj + '/' + plane, event.timestamp);
        }
      },

      'i915_flip_complete': {
        format: /plane=(\d+), obj=(.+)/,
        handler: function(importer, event) {
          var plane = parseInt(event[1]);
          var obj = event[2];
          // use i915_obj_plane?
          var kthread = importer.getOrCreateKernelThread('i915_flip', 0, 2);
          importer.closeSlice(kthread, event.timestamp, {
                obj: obj,
                plane: plane
              });
        }
      },

      'tracing_mark_write': {
        handler: function(importer, event) {
          var eventData = traceEventClockSyncRE.exec(event.info);
          if (eventData) {
            importer.clockSyncRecords_.push({
              perfTS: event.timestamp,
              parentTS: eventData[1] * 1000
            });
          } else {
            var eventData = event.info.split('|')
            switch (eventData[0]) {
              case 'B':
                var pid = parseInt(eventData[1]);
                var name = eventData[2];
                importer.processBegin(event.taskId, name, event.timestamp, pid);
                break;
              case 'E':
                importer.processEnd(event.taskId, event.timestamp);
                break;
              case 'C':
                var pid = parseInt(eventData[1]);
                var name = eventData[2];
                var value = parseInt(eventData[3]);
                importer.processCounter(name, event.timestamp, value, pid);
                break;
              default:
                importer.malformedEvent(event.name);
                break;
            }
          }
        }
      },
    }
  };

  // NB: old-style trace markers; deprecated
  LinuxPerfImporter.prototype.eventDefinitions['0'] =
      LinuxPerfImporter.prototype.eventDefinitions['tracing_mark_write'];

  TestExports.eventDefinitions = LinuxPerfImporter.prototype.eventDefinitions;

  tracing.TimelineModel.registerImporter(LinuxPerfImporter);

  return {
    LinuxPerfImporter: LinuxPerfImporter,
    _LinuxPerfImporterTestExports: TestExports
  };

});
