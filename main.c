#define _GNU_SOURCE
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERROR_LOG(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

const char ASCII_CHARS[] = {'.', ',', '-', '~', ':', ';',
                            '=', '!', '*', '#', '$', '@'};

static volatile sig_atomic_t keep_running = 1;

void handle_sigint(int sig)
{
    (void)sig;
    keep_running = 0;
}

typedef struct
{
    float         brightness;
    unsigned char r;
    unsigned char g;
    unsigned char b;
} Pixel;

bool read_safe_line(char* buffer, size_t size)
{
    if (fgets(buffer, (int)size, stdin) == NULL)
    {
        return false;
    }
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
    {
        buffer[len - 1] = '\0';
    }
    else
    {
        int c;
        while ((c = getchar()) != '\n' && c != EOF)
            ;
    }
    return true;
}

float calculate_brightness(unsigned char red, unsigned char green,
                           unsigned char blue)
{
    float linear_brightness = (0.2126f * (float)red + 0.7152f * (float)green +
                               0.0722f * (float)blue) /
                              255.0f;
    return cbrt(linear_brightness);
}

int convert_image_to_ascii(const unsigned char* image, Pixel* ascii, int width,
                           int height, int target_width, int target_height)
{
    float step_x = (float)width / (float)target_width;
    float step_y = (float)height / (float)target_height;

    for (int y = 0; y < target_height; ++y)
    {
        for (int x = 0; x < target_width; ++x)
        {
            int orig_x      = (int)((float)x * step_x);
            int orig_y      = (int)((float)y * step_y);
            int pixel_index = (orig_y * width + orig_x) * 3;

            unsigned char r = image[pixel_index];
            unsigned char g = image[pixel_index + 1];
            unsigned char b = image[pixel_index + 2];

            float brightness = calculate_brightness(r, g, b);
            ascii[x + y * target_width] =
                (Pixel){.brightness = brightness, .r = r, .g = g, .b = b};
        }
    }
    return target_height * target_width;
}

void print_ascii(const Pixel* ascii, int ascii_len, int target_width)
{
    size_t est_size =
        (size_t)ascii_len * 32 + (size_t)(ascii_len / target_width) + 10;
    char* frame_buffer = (char*)malloc(est_size);
    if (!frame_buffer)
    {
        return;
    }

    char* ptr    = frame_buffer;
    int   column = 0;
    for (int i = 0; i < ascii_len; ++i)
    {
        Pixel p     = ascii[i];
        int   index = (int)(p.brightness * 11.0f);

        if (index < 0)
        {
            index = 0;
        }
        if (index > 11)
        {
            index = 11;
        }

        ptr += sprintf(ptr, "\033[38;2;%d;%d;%dm%c", p.r, p.g, p.b,
                       ASCII_CHARS[index]);

        column++;
        if (column == target_width)
        {
            *ptr++ = '\n';
            column = 0;
        }
    }

    ptr += sprintf(ptr, "\033[0m");
    fwrite(frame_buffer, 1, (size_t)(ptr - frame_buffer), stdout);
    free(frame_buffer);
}

void parse_args(int argc, char** argv, int* width_out, int* height_out,
                int* fps_out)
{
    if (argc != 2)
    {
        ERROR_LOG("Usage: ./main <WIDTHxHEIGHT:FPS> (e.g., 300x200:30)");
        exit(1);
    }

    char* buffer = argv[1];
    char* end_ptr;

    long w = strtol(buffer, &end_ptr, 10);
    if (end_ptr == buffer || *end_ptr != 'x' || w <= 0)
    {
        ERROR_LOG("Invalid width. Expected format: WIDTHxHEIGHT:FPS");
        exit(1);
    }

    char* height_start = end_ptr + 1;
    long  h            = strtol(height_start, &end_ptr, 10);
    if (end_ptr == height_start || *end_ptr != ':' || h <= 0)
    {
        ERROR_LOG("Invalid height. Expected format: WIDTHxHEIGHT:FPS");
        exit(1);
    }

    char* fps_start = end_ptr + 1;
    long  f         = strtol(fps_start, &end_ptr, 10);
    if (end_ptr == fps_start || *end_ptr != '\0' || f <= 0)
    {
        ERROR_LOG("Invalid FPS. Expected format: WIDTHxHEIGHT:FPS");
        exit(1);
    }

    *width_out  = (int)w;
    *height_out = (int)h;
    *fps_out    = (int)f;
}

int main(int argc, char** argv)
{
    signal(SIGINT, handle_sigint);

    int width;
    int height;
    int fps;
    parse_args(argc, argv, &width, &height, &fps);

    int target_width  = width;
    int target_height = height;

    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0 &&
        w.ws_row > 0)
    {
        target_width  = (w.ws_col - 1 < width) ? w.ws_col - 1 : width;
        target_height = (w.ws_row - 1 < height) ? w.ws_row - 1 : height;
    }

    size_t         frame_size = (size_t)width * (size_t)height * 3;
    unsigned char* raw_image  = (unsigned char*)malloc(frame_size);
    Pixel*         ascii_pixels =
        (Pixel*)malloc((size_t)width * (size_t)height * sizeof(Pixel));

    if (!raw_image || !ascii_pixels)
    {
        ERROR_LOG("Memory allocation failed");
        return 1;
    }

    printf("\033[?25l");
    printf("\033[H");

    while (keep_running && fread(raw_image, 1, frame_size, stdin) == frame_size)
    {
        if (!keep_running)
        {
            break;
        }

        int ascii_len =
            convert_image_to_ascii(raw_image, ascii_pixels, width, height,
                                   target_width, target_height);

        printf("\033[H");
        print_ascii(ascii_pixels, ascii_len, target_width);
        fflush(stdout);
    }

    printf("\033[?25h");

    free(raw_image);
    free(ascii_pixels);

    return 0;
}
