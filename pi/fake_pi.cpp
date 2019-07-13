//
// Emulate Raspberry Pi specific code
//

static const uint32_t chunck_size = 4096;

uint32_t buf_len = 256*1024*1024;
uint32_t buf_ptr;
uint16_t *stream_buf;

static void cleanup_smi()
{
    free(stream_buf);
}

static void cleanup_smi_and_exit(int sig)
{
  printf("\nExiting with error; caught signal: %i\n", sig);
  cleanup_smi();
  exit(1);
}

static bool start_smi_dma()
{
    stream_buf = (uint16_t *)malloc(buf_len);
    if (stream_buf == 0)
    {
        fprintf(stderr, "Buffer malloc failed\n");
        return false;
    }

    FILE *file;
    file = fopen("c64_pi_dump.bin", "r");
    if (file != NULL)
    {
        if (fread(stream_buf, buf_len, 1, file) != 1)
        {
            fprintf(stderr, "Failed to read data from file\n");
            return false;
        }
        fclose(file);
    }
    else
    {
        fprintf(stderr, "Failed to open c64_pi_dump.bin for reading. %s\n",
            strerror(errno));
        return false;
    }

    buf_ptr = 0;
    return true;
}

static uint16_t *get_next_smi_chunk()
{
    uint16_t *result;

    if (buf_ptr < buf_len/sizeof(uint16_t))
    {
        result = stream_buf + buf_ptr;
        buf_ptr += chunck_size/sizeof(uint16_t);
    }
    else
    {
        printf("End of stream\n");
        cleanup_smi();
        exit(0);
    }

    return result;
}
