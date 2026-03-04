import Speaker from "speaker";

let i = 0;
let dataLen = 0;

const PACKET_LEN_MS = 0.02; // 20ms
const TOTAL_PACKETS = 1 / PACKET_LEN_MS;
const SAMPLES_PER_PACKET = PACKET_LEN_MS * 48000;

const speaker = new Speaker({
  sampleRate: 48000,
  channels: 1,
  bitDepth: 32,
});

const server = await Bun.udpSocket({
  port: 3000,
  socket: {
    data(_, buf, port, addr) {
      console.log(`message from ${addr}:${port}:`);

      dataLen += buf.byteLength;
      console.log(++i, buf.byteLength, dataLen);

      speaker.write(buf);
    },
  },
});

console.log(server.address);
