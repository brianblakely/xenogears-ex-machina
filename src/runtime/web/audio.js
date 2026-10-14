// Web Audio output, started by a user gesture. The game's SPU produces 44.1 kHz
// stereo; each animation frame the page takes what the frames produced
// (app.take_audio, already mixed by the volume and output settings) and posts
// it to an AudioWorklet's ring. The context runs at the SPU's rate and the
// browser resamples to the device. No SharedArrayBuffer is needed.

import { audio_sample_rate } from './pkg/xem_web.js';

export class AudioOut {
  constructor(app) {
    this.app = app;
    this.context = null;
    this.node = null;
    this.stats = { buffered: 0, played: 0, underruns: 0, overflows: 0 };
    this.posted = 0;
    this.error = null;
    this.revision = null;
  }

  /** Call from a user gesture (click, key): creates or resumes the context. */
  async start() {
    try {
      // Created synchronously inside the gesture, before any await.
      this.context ??= new AudioContext({ sampleRate: audio_sample_rate(), latencyHint: 'interactive' });
      const resumed = this.context.resume();
      if (!this.node) {
        await this.context.audioWorklet.addModule(new URL('./audio-worklet.js', import.meta.url));
        this.node = new AudioWorkletNode(this.context, 'xem-output', { outputChannelCount: [2] });
        this.node.port.onmessage = (event) => {
          this.stats = event.data;
        };
        this.node.connect(this.context.destination);
      }
      await resumed;
    } catch (error) {
      this.error = String(error);
      throw error;
    }
  }

  async suspend() {
    await this.context?.suspend();
  }

  reset() { this.node?.port.postMessage({ reset: true }); this.revision = null; }

  /** Post the game's new samples to the worklet (dropped while nothing plays them). */
  pump() {
    const revision = this.app.timeline_revision();
    if (revision !== this.revision) {
      this.revision = revision;
      this.node?.port.postMessage({ reset: true });
    }
    const samples = this.app.take_audio();
    if (samples.length === 0 || this.context?.state !== 'running' || !this.node) return;
    this.posted += samples.length / 2;
    // A held rewind's moments of sound, each shorter than the pre-roll, play at once.
    if (this.app.rewinding()) this.node.port.postMessage({ samples, prime: true }, [samples.buffer]);
    else this.node.port.postMessage(samples, [samples.buffer]);
  }

  status() {
    return {
      state: this.context?.state ?? 'not started',
      sampleRate: this.context?.sampleRate ?? null,
      posted: this.posted,
      ...this.stats,
      error: this.error,
    };
  }
}
