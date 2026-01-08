<div align="center">
  <img src="resources/app.ico" width="128" height="128" />
  <h1>FolkADB</h1>
  <p>
    <strong>简体中文</strong> | <a href="README_EN.md">English</a>
  </p>
</div>

FolkADB 是一个便携式的 Android 调试桥 (ADB) 和 Fastboot 工具，旨在简化 Android 设备的管理和调试过程。它采用 C 语言编写，无需繁琐的环境配置，开箱即用。

## 核心功能

### 1. 便携与自动配置
程序启动时会自动提取并配置所需的 ADB 和 Fastboot 环境，无需用户手动设置系统环境变量。支持自动检测设备模式（ADB/Fastboot）并自动连接。输入 `help` 回车可以查看所有命令。

### 2. 交互式命令行与命令简化
FolkADB 提供了一个类似 Shell 的交互环境，你不再需要每次都输入 `adb` 或者 `fastboot` 前缀，当然也兼容直接输入 `adb` 或者 `fastboot` 前缀的命令。
*   **直接命令**: 直接输入 `ls`, `rm`, `install`, `shell` 等命令。
*   **快捷键**: 提供数字快捷键（1-9）快速执行常用操作，在命令行输入 `s` 回车可以查看所有快捷键。
*   **Shell模式**: 在 ADB 模式下输入 `shell` 回车可以进入 Shell 模式，在 Shell 模式下可以直接输入 Shell 命令，输入 `exit` 回车可以退出 Shell 模式。在此基础上输入 `su` 回车可以提权到 root shell，但是你也可以使用 `sudo <command>` 来临时提权执行命令，比如 `sudo /data/adb/apd module install 1.zip`，这个命令就是在 shell 中临时提权调用 APatch 的守护进程来安装模块。

### 3. 高级功能详解 (请在 root 管理器给 shell 程序授权 root 权限)

#### ⚡ Sudo 提权
在交互式 Shell 中，内置了 `sudo` 命令。
*   **用法**: `sudo <command>`
*   **作用**: 自动通过 `su -c` 执行命令，方便在未 root 的 shell 环境中快速执行需要 root 权限的指令。

#### 📥 DLI 下载安装
内置 `dli` (Download & Install) 命令，支持从 URL 直接安装模块。
*   **用法**: `dli <url>`
*   **作用**: 自动下载文件，推送到设备，并根据 Root 方案进行安装。

#### 🧩 模块安装 (Magisk/KernelSU/APatch)
支持自动化安装 Root 模块。
*   **拖入安装**: 直接将模块压缩包 (`.zip`) 拖入 `FolkAdb.exe` 图标，程序会自动识别模块类型并调用相应的安装逻辑。
*   **智能识别**: 自动识别 Magisk、KernelSU 或 APatch/FolkPatch 环境并调用相应的安装逻辑。

#### 📱 APK 拖入安装
*   **操作**: 直接将 `.apk` 文件拖入 `FolkAdb.exe` 图标或运行窗口 (窗口方案只能单 APK 安装，图标支持多 APK 安装)。
*   **流程**: 程序会自动识别 APK 文件，并询问是否安装，确认后即可自动完成安装过程。

#### 📂 文件传输
*   **拖入传输**: 将普通文件拖入 `FolkAdb.exe` 图标，会自动推送到设备的 `/storage/emulated/0/` 目录，支持多选。如果判断 ZIP 文件是模块会询问是否安装。

## 构建与运行

### 环境要求
*   Windows 操作系统
*   C 编译器 (如 GCC/MinGW)

### 构建
在项目根目录下运行：
```cmd
build.bat
```

### 运行
```cmd
run.bat
```
或直接运行 `build/FolkAdb.exe`。

## 许可证
本项目采用 Apache 2.0 许可证。详情请参阅 [LICENSE](LICENSE) 文件。
