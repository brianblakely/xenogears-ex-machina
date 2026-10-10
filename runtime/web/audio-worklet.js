// The audio output: a ring of interleaved stereo samples the runtime fills.
// When it runs low it asks the page for more; no SharedArrayBuffer is needed.

const CAPACITY = 16384; // frames, about 0.34 s at 48 kHz
const LOW = 4800; // ask for more below this many frames
const REQUEST = 2400;

class XemOutput extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ring = new Float32Array(CAPACITY * 2);
    this.read = 0;
    this.buffered = 0;
    this.requested = 0;
    this.played = 0;
    this.underruns = 0;
    this.port.onmessage = (event) => this.push(event.data);
    this.ask();
  }

  push(samples) {
    const frames = Math.min(samples.length / 2, CAPACITY - this.buffered);
    let write = (this.read + this.buffered) % CAPACITY;
    for (let i = 0; i < frames; i++) {
      this.ring[write * 2] = samples[i * 2];
      this.ring[write * 2 + 1] = samples[i * 2 + 1];
      write = (write + 1) % CAPACITY;
    }
    this.buffered += frames;
    this.requested = Math.max(0, this.requested - samples.length / 2);
  }

  ask() {
    const need = this.buffered + this.requested < LOW ? REQUEST : 0;
    if (need || this.played - (this.reported ?? 0) >= REQUEST) {
      this.requested += need;
      this.reported = this.played;
      this.port.postMessage({ need, buffered: this.buffered, played: this.played, underruns: this.underruns });
    }
  }

  process(_inputs, outputs) {
    const [left, right] = outputs[0];
    for (let i = 0; i < left.length; i++) {
      if (this.buffered > 0) {
        left[i] = this.ring[this.read * 2];
        right[i] = this.ring[this.read * 2 + 1];
        this.read = (this.read + 1) % CAPACITY;
        this.buffered--;
        this.played++;
      } else {
        left[i] = right[i] = 0;
        this.underruns++;
      }
    }
    this.ask();
    return true;
  }
}

registerProcessor('xem-output', XemOutput);
