#include <stdio.h>
#include <stdint.h>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>

typedef int32_t i32;

#define NUM_CHANNELS 1
#define BIT_DEPTH ma_format_s32
#define SAMPLE_RATE 48000

#define RECORD_BUFFER_LENGTH SAMPLE_RATE * NUM_CHANNELS * (5)

i32 buffer[RECORD_BUFFER_LENGTH];
atomic_uint j = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void data_callback(ma_device *device, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    unsigned int index =
        atomic_load_explicit(&j, memory_order_relaxed);

    for (ma_uint32 i = 0;
         i < frameCount && index < RECORD_BUFFER_LENGTH;
         i++)
    {
        buffer[index++] = ((i32 *)pInput)[i];
    }

    atomic_store_explicit(&j, index, memory_order_release);

    if (index >= RECORD_BUFFER_LENGTH)
    {
        printf("Index: %d\n", index);
        pthread_cond_signal(&cond);
    }

    (void)device;
    (void)pOutput;
}

int main()
{
    getchar();
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.sampleRate = SAMPLE_RATE;
    config.capture.channels = NUM_CHANNELS;
    config.capture.format = BIT_DEPTH;
    config.dataCallback = data_callback;

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS)
    {
        perror("device init issue");
        return -1;
    }

    printf("Device Info: ");
    printf("%d, %d, %d, %s\n", device.sampleRate, device.capture.channels, device.capture.format, device.capture.name);

    if (ma_device_start(&device) != MA_SUCCESS)
    {
        perror("device start");
        return -1;
    }

    printf("Device started\n");

    pthread_mutex_lock(&mutex);
    while (atomic_load_explicit(&j, memory_order_acquire) < RECORD_BUFFER_LENGTH)
    {
        pthread_cond_wait(&cond, &mutex);
    }

    int index = atomic_load_explicit(&j, memory_order_acquire);
    printf("closing device at index: %d\n", index);

    pthread_mutex_unlock(&mutex);

    if (ma_device_stop(&device) != MA_SUCCESS)
    {
        perror("device stop error");
        return -1;
    }

    int fd = open("sample.pcm", O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0)
    {
        perror("file");
        return -1;
    }

    unsigned long offset = 0;

    while (offset < (RECORD_BUFFER_LENGTH * sizeof(i32)))
    {
        int n = write(fd, ((char *)buffer) + offset, (RECORD_BUFFER_LENGTH * sizeof(i32)) - offset);
        if (n > 0)
        {
            offset += n;
        }
        else
        {
            printf("n == 0");
            return -1;
        }
    }
    printf("Written to disk");

    ma_device_uninit(&device);
    return 0;
}
