## 20211119

直接透传 RGBA 32 位数据给 ffmpeg, 不要在 superstream 容器中做转换；

``` sh
./ffmpeg -f superstream -i /dev/binder -vcodec libopenh264 -r 30 -f h264 "udp://224.224.224.224:6666"
```
可以使用上面命令直接发送组播；

也可以利用：https://hub.docker.com/search?q=nginx-rtmp&type=image 等搭建直播推流服务器测试；

在使用 `libopen264` 进行编码情况下，通过：

``` sh
./ffmpeg -f superstream -i /dev/binder  -r 30  -y -vcodec libopenh264 -f flv rtmp://10.30.11.104:1935/live/test
ffplay -fflags nobuffer rtmp://10.30.11.104:1935/live/test
```
时延要比 ardc 强，但是 hls 播放还是存在时延

## 20211117

手动编译 FFmpeg 之后通过如下命令可以进行录制了：

``` sh
./ffmpeg -f superstream -i /dev/binder -vcodec libx264 -r 30 -report output.mp4

```

## 20211116
该版本通过 `./bctest superstream` 已经可以从 Docker 中 `SuperStream` 接受视频数据并且保存到文件；

生成的原始 RGBA 数据可以通过网站 https://rawpixels.net/ 来进行解码

