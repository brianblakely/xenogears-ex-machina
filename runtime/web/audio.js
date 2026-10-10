// Web Audio output, started by a user gesture. The runtime renders the samples
// (app.audio_render); an AudioWorklet plays them from its ring and asks for
// more when it runs low.

export class AudioOut {
  constructor(app) {
    this.app = app;
    this.context = null;
    this.node = null;
    this.stats = { buffered: 0, played: 0, underruns: 0 };
    this.error = null;
  }

  /** Call from a user gesture (click, key): creates or resumes the context. */
  async start() {
    try {
      // Created synchronously inside the gesture, before any await.
      this.context ??= new AudioContext({ latencyHint: 'interactive' });
      const resumed = this.context.resume();
      if (!this.node) {
        await this.context.audioWorklet.addModule(new URL('./audio-worklet.js', import.meta.url));
        this.node = new AudioWorkletNode(this.context, 'xem-output', { outputChannelCount: [2] });
        this.node.port.onmessage = (event) => this.feed(event.data);
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

  feed({ need, buffered, played, underruns }) {
    this.stats = { buffered, played, underruns };
    if (need > 0) {
      const samples = this.app.audio_render(need, this.context.sampleRate);
      this.node.port.postMessage(samples, [samples.buffer]);
    }
  }

  status() {
    return {
      state: this.context?.state ?? 'not started',
      sampleRate: this.context?.sampleRate ?? null,
      ...this.stats,
      error: this.error,
    };
  }
}
