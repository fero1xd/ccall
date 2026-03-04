import Speaker from "speaker";
import OpusScript from "opusscript";

let i = 0;
let dataLen = 0;

const SAMPLE_RATE = 48000;
const PACKET_LEN_MS = 0.02; // 20ms
const TOTAL_PACKETS = 1 / PACKET_LEN_MS;

const FRAME_SIZE = PACKET_LEN_MS * SAMPLE_RATE;

const speaker = new Speaker({
  sampleRate: 48000,
  channels: 1,
  bitDepth: 16,
});

const encoder = new OpusScript(SAMPLE_RATE, 1, OpusScript.Application.VOIP);
const pcmOut = new Int16Array(FRAME_SIZE * 1);

const server = await Bun.udpSocket({
  port: 3000,
  socket: {
    data(_, buf, port, addr) {
      console.log(`message from ${addr}:${port}:`);

      dataLen += buf.byteLength;

      const decoded = encoder.decode(buf);
      console.log(++i, buf.byteLength, dataLen, decoded.byteLength);

      // speaker.write(new Int16Array(pcmOut.subarray(0, decodedSize)));

      // const out = Buffer.allocUnsafe(decodedSize);
      // out.set(new Uint8Array(pcmOut.buffer, 0, decodedSize));
      speaker.write(decoded);
    },
  },
});

console.log(server.address);
