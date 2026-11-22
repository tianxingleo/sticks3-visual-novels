# Yuiline 版使用指南

第一章共 78 页，保留上游网页的背景和立绘切换、爵士配乐、摊位环境音及河水声。第一章可单机游玩，后续分支由电脑端通过 DeepSeek 生成。

## 导入素材并刷写

先准备 Python、Pillow 和已加入 `PATH` 的 FFmpeg。以下命令从仓库根目录运行，从固定版本的上游源码生成本地 `firmware/YuilineStory/Generated.cpp`。

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install Pillow
git clone https://github.com/Yuiline/galgame-by-yuiline.git .build/yuiline-upstream
git -C .build/yuiline-upstream checkout 768123ba8fe67dc72fac3fa9b420a9e514b45d08
.\.venv\Scripts\python.exe tools/import_yuiline.py .build/yuiline-upstream firmware/YuilineStory

.\arduino.ps1 -Action Compile -Sketch YuilineStory
.\arduino.ps1 -Action Upload -Sketch YuilineStory -Port COM6
```

将 `COM6` 换成设备的实际串口。上游对白从 `game.html` 提取，图片缩放至 240 × 135，音频转换为 12 kHz 单声道。生成文件随固件一起编译。维护时若更新上游版本，需核对 `firmware/YuilineStory/Timeline.h` 中以 0 开始的触发位置。 [上游固定版本](https://github.com/Yuiline/galgame-by-yuiline/tree/768123ba8fe67dc72fac3fa9b420a9e514b45d08)

## 启动电脑端续写

在仓库根目录运行以下命令，在提示中输入 DeepSeek API 密钥。启动后保持终端运行和 USB 数据线连接。

```powershell
.\.venv\Scripts\python.exe -m pip install -r host/requirements.txt
$env:GALGAME_DEEPSEEK_API_KEY = [System.Net.NetworkCredential]::new('', (Read-Host 'DeepSeek API 密钥' -AsSecureString)).Password
.\.venv\Scripts\python.exe -X utf8 host/galgame.py --port COM6
```

如需指定账户可用的模型，可传入 `--model` 或设置 `GALGAME_MODEL`。同一串口同时只允许一个程序使用。刷写固件前，先在续写服务终端按 **Ctrl+C** 停止服务。

## 按键与续写行为

| 画面 | A 键 | B 键 |
| --- | --- | --- |
| 标题 | 开始第一章 | 跳到第一章结尾开始续写 |
| 第一章与续写对白 | 翻页 | 返回标题 |
| 续写选项 | 切换选项 | 点按确认，长按返回标题 |
| 等待生成 |  | 返回标题 |
| 生成失败 | 重试 | 返回标题 |

第一章剩余约 20 页时开始预生成续写。显示新剧情时，电脑会并行请求两个选项的后续，每个分支各产生一次 API 调用。返回标题后重新开始会新建故事。若电脑端服务重启，进行中的续写需要从标题重新开始。
