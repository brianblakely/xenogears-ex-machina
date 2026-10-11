// The audio output: a ring of interleaved stereo samples the page posts after
// each animation frame. Latency stays bounded: when more than LIMIT frames are
// waiting (the page ran ahead of the audio clock) the oldest are dropped; when
// the ring runs dry it plays silence. No SharedArrayBuffer is needed.

const CAPACITY = 16384; // frames, about 0.37 s at 44.1 kHz
const LIMIT = 8192; // frames kept waiting at most, about 0.19 s
const REPORT = 2048; // report about every this many frames played

class XemOutput extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ring = new Float32Array(CAPACITY * 2);
    this.read = 0;
    this.buffered = 0;
    this.played = 0;
    this.underruns = 0;
    this.overflows = 0;
    this.reported = 0;
    this.port.onmessage = (event) => {
      if (event.data.reset) { this.read = 0; this.buffered = 0; }
      else this.push(event.data);
    };
  }

  push(samples) {
    let frames = samples.length / 2;
    let from = 0;
    if (frames > LIMIT) {
      from = frames - LIMIT;
      this.overflows += from;
      frames = LIMIT;
    }
    const excess = this.buffered + frames - LIMIT;
    if (excess > 0) {
      this.read = (this.read + excess) % CAPACITY;
      this.buffered -= excess;
      this.overflows += excess;
    }
    let write = (this.read + this.buffered) % CAPACITY;
    for (let i = from; i < from + frames; i++) {
      this.ring[write * 2] = samples[i * 2];
      this.ring[write * 2 + 1] = samples[i * 2 + 1];
      write = (write + 1) % CAPACITY;
    }
    this.buffered += frames;
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
    if (this.played + this.underruns - this.reported >= REPORT) {
      this.reported = this.played + this.underruns;
      this.port.postMessage({ buffered: this.buffered, played: this.played, underruns: this.underruns,
                              overflows: this.overflows });
    }
    return true;
  }
}

registerProcessor('xem-output', XemOutput);
