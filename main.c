#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <opus/opus.h>

typedef int32_t i32;
typedef uint32_t u32;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define NUM_CHANNELS 1
#define BIT_DEPTH ma_format_s32
#define SAMPLE_RATE 48000
#define RECORD_BUFFER_LENGTH SAMPLE_RATE * NUM_CHANNELS * (5)

ma_pcm_rb rb;
int sockfd;
struct sockaddr_in dest_addr;
socklen_t addrlen = sizeof(dest_addr);
OpusEncoder *enc;

void data_callback(ma_device *device, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    u32 required_frames = frameCount;
    void *buf = NULL;
    assert(ma_pcm_rb_acquire_write(&rb, &required_frames, &buf) == MA_SUCCESS);
    assert(buf != NULL);
    assert(required_frames == frameCount);

    memcpy(buf, pInput, required_frames * sizeof(i32));

    // printf("required_frames: %d, ", required_frames);
    // printf("available write: %d\n", ma_pcm_rb_available_write(&rb));

    assert(ma_pcm_rb_commit_write(&rb, required_frames) == MA_SUCCESS);
    // unsigned int index =
    //     atomic_load_explicit(&j, memory_order_relaxed);

    // if (index > RECORD_BUFFER_LENGTH)
    //     return;

    // u32 len = MIN(RECORD_BUFFER_LENGTH - index, frameCount);
    // // memcpy(&buffer[index], pInput, len * sizeof(i32));
    // index += len;
    // atomic_store_explicit(&j, index, memory_order_release);

    // if (index >= RECORD_BUFFER_LENGTH)
    // {
    //     // Signal once. Avoid printf in RT thread.
    //     pthread_cond_signal(&cond);
    // }

    (void)device;
    (void)pOutput;
}

unsigned char opusPacket[4000];
void playback_callback(ma_device *device, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    (void)pInput;
    (void)pOutput;
    (void)device;

    u32 required_frames = frameCount;
    void *buf = NULL;

    assert(ma_pcm_rb_acquire_read(&rb, &required_frames, &buf) == MA_SUCCESS);
    if (required_frames != frameCount)
    {
        printf("not enough frames available to read\n");
        return;
    }

    // printf("frameCount: %d, available read: %d\n", frameCount, ma_pcm_rb_available_read(&rb));

    assert(buf != NULL);
    // memcpy(pOutput, buf, required_frames * sizeof(i32));

    ma_pcm_s32_to_s16(buf, buf, required_frames, 0);

    i32 n = opus_encode(enc, (opus_int16 *)buf, required_frames, opusPacket, 4000);
    ssize_t bytes_sent = sendto(sockfd,
                                opusPacket,
                                n, 0,
                                (const struct sockaddr *)&dest_addr,
                                addrlen);

    printf("norma_size:%zd frames:%d opus_encoded: %d\n", required_frames * sizeof(int16_t), required_frames, n);
    assert(bytes_sent == n);
    assert(ma_pcm_rb_commit_read(&rb, required_frames) == MA_SUCCESS);
}

int main()
{
    assert(ma_pcm_rb_init(BIT_DEPTH, NUM_CHANNELS, RECORD_BUFFER_LENGTH, NULL, NULL, &rb) == MA_SUCCESS);
    int err;

    enc = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &err);
    assert(err == OPUS_OK);

    // setup udp socket
    assert((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0);

    memset(&dest_addr, 0, sizeof(dest_addr));
    // setup dest address
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(3000);
    dest_addr.sin_addr.s_addr = inet_addr("0.0.0.0");

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.sampleRate = SAMPLE_RATE;
    config.capture.channels = NUM_CHANNELS;
    config.capture.format = BIT_DEPTH;
    config.dataCallback = data_callback;
    config.periodSizeInMilliseconds = 20;

    ma_device_config playback_config = ma_device_config_init(ma_device_type_playback);
    playback_config.sampleRate = SAMPLE_RATE;
    playback_config.playback.channels = NUM_CHANNELS;
    playback_config.playback.format = BIT_DEPTH;
    playback_config.dataCallback = playback_callback;
    playback_config.periodSizeInMilliseconds = 20;

    ma_device device;
    ma_device playback_device;
    assert(ma_device_init(NULL, &config, &device) == MA_SUCCESS);
    assert(ma_device_init(NULL, &playback_config, &playback_device) == MA_SUCCESS);

    printf("Capture device Info: ");
    printf("%d, %d, %d, %s\n", device.sampleRate, device.capture.channels, device.capture.format, device.capture.name);
    printf("Playback device Info: ");
    printf("%d, %d, %d, %s\n", playback_device.sampleRate, playback_device.playback.channels, playback_device.playback.format, playback_device.playback.name);

    assert(ma_device_start(&device) == MA_SUCCESS);
    assert(ma_device_start(&playback_device) == MA_SUCCESS);
    printf("Capture and Playback device started\n");

    getchar();

    assert(ma_device_stop(&device) == MA_SUCCESS);
    assert(ma_device_stop(&playback_device) == MA_SUCCESS);

    ma_device_uninit(&device);
    ma_device_uninit(&playback_device);
    ma_pcm_rb_uninit(&rb);
    close(sockfd);
    return 0;
}
