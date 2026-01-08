<div align="center">
  <img src="resources/app.ico" width="128" height="128" />
  <h1>FolkADB</h1>
  <p>
    <a href="README.md">简体中文</a> | <strong>English</strong>
  </p>
</div>

FolkADB is a portable Android Debug Bridge (ADB) and Fastboot utility designed to simplify Android device management and debugging. Written in C, it requires no cumbersome environment configuration and works out of the box.

## Core Features

### 1. Portable & Auto-Configuration
The program automatically extracts and configures the required ADB and Fastboot environment upon startup, eliminating the need for users to manually set system environment variables. It supports automatic detection of device modes (ADB/Fastboot) and automatic connection. Type `help` and press Enter to view all commands.

### 2. Interactive CLI & Command Simplification
FolkADB provides a shell-like interactive environment where you no longer need to type the `adb` or `fastboot` prefix every time, although standard commands with `adb` or `fastboot` prefixes are also supported.
*   **Direct Commands**: Directly input commands like `ls`, `rm`, `install`, `shell`, etc.
*   **Shortcuts**: Provides numeric shortcuts (1-9) for quick execution of common operations. Type `s` and press Enter in the command line to view all shortcuts.
*   **Shell Mode**: In ADB mode, type `shell` and press Enter to enter Shell mode. In Shell mode, you can directly input Shell commands. Type `exit` and press Enter to exit Shell mode. While in Shell mode, you can type `su` and press Enter to elevate to a root shell. Alternatively, you can use `sudo <command>` to temporarily elevate privileges for a single command. For example, `sudo /data/adb/apd module install 1.zip` uses this temporary privilege elevation to call the APatch daemon for module installation.

### 3. Advanced Features (Please grant root permission to the shell app in your Root Manager)

#### ⚡ Sudo Privilege Escalation
In the interactive Shell, a `sudo` command is built-in.
*   **Usage**: `sudo <command>`
*   **Function**: Automatically executes the command via `su -c`, facilitating the quick execution of commands requiring root privileges in a non-root shell environment.

#### 📥 DLI Download & Install
Built-in `dli` (Download & Install) command supports installing modules directly from a URL.
*   **Usage**: `dli <url>`
*   **Function**: Automatically downloads the file, pushes it to the device, and installs it according to the detected Root solution.

#### 🧩 Module Installation (Magisk/KernelSU/APatch)
Supports automated installation of Root modules.
*   **Drag & Drop Install**: Drag a module zip file (`.zip`) directly onto the `FolkAdb.exe` icon. The program will automatically identify the module type and invoke the corresponding installation logic.
*   **Smart Detection**: Automatically identifies Magisk, KernelSU, or APatch/FolkPatch environments and calls the appropriate installation logic.

#### 📱 APK Drag & Drop Install
*   **Operation**: Drag an `.apk` file directly onto the `FolkAdb.exe` icon or the running window (The window method supports single APK installation, while the icon method supports multiple APKs).
*   **Process**: The program automatically identifies the APK file and asks whether to install it. Upon confirmation, the installation process completes automatically.

#### 📂 File Transfer
*   **Drag & Drop Transfer**: Drag ordinary files onto the `FolkAdb.exe` icon to automatically push them to the device's `/storage/emulated/0/` directory. Supports multiple file selection. If a ZIP file is detected as a module, you will be asked whether to install it.

## Build and Run

### Requirements
*   Windows OS
*   C Compiler (e.g., GCC/MinGW)

### Build
Run the following command in the project root directory:
```cmd
build.bat
```

### Run
```cmd
run.bat
```
Or run `build/FolkAdb.exe` directly.

## License
This project is licensed under the Apache 2.0 License. See the [LICENSE](LICENSE) file for details.
