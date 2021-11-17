## 20211117

手动编译 FFmpeg 之后通过如下命令可以进行录制了：

``` sh
./ffmpeg -f superstream -i /dev/binder -vcodec libx264 -r 30 -report output.mp4

```

## 20211116
该版本通过 `./bctest superstream` 已经可以从 Docker 中 `SuperStream` 接受视频数据并且保存到文件；

生成的原始 RGBA 数据可以通过网站 https://rawpixels.net/ 来进行解码

