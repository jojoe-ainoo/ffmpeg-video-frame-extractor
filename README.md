# Assignment 3

This is a C project using the `ffmpeg` graphics library to build an application

## Project Brief

This project uses ffmpeg libraries to extract 10 frames from a video and convert them to gray scale images saved in the same directory

## Contribution

The application was built by:

```
- Emmanuel Ainoo
- Ayomide Oduba
```

## Requirements

- [`gcc C program compiler`](https://gcc.gnu.org)
- [`ffmpeg`]([https://ffmpeg.org)]

## Usage

Compile with gcc & gtk3 flags on terminal:

```shell
gcc -I/opt/homebrew/Cellar/ffmpeg/5.1.2/include -L/opt/homebrew/Cellar/ffmpeg/5.1.2/lib -lavcodec -lavformat -lavutil -lswscale -o A3 A3.c
```

Run File:

```shell
./A3 sample.mpg
```

Open A3 directory to locate the 10 frames
