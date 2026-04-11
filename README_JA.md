<div align="center">
  <img src="resources/app.ico" width="128" height="128" />
  <h1>FolkADB</h1>
  <p>
    <a href="README.md">简体中文</a> | <a href="README_EN.md">English</a> | <strong>日本語</strong>
  </p>
</div>

FolkADB は Android デバイスの管理とデバッグを簡素化するために設計された、ポータブルな Android デバッグブリッジ (ADB) および Fastboot ユーティリティです。C 言語で書かれており、面倒な環境設定が不要かつ、すぐに使用可能になっています。

## コア機能

### 1. ポータブルと自動構成
このプログラムは自動時に必要な ADB および Fastboot の環境を自動的に抽出して構成するため、ユーザーがシステム環境変数を手動で設定する必要がありません。デバイスモード (ADB/Fastboot) の自動検出と自動接続をサポートしています。すべてのコマンドを表示するには `help` と入力して Enter キーを押してください。

### 2. 対話型 CLI とコマンドの簡素化
FolkADB はシェルに似た対話型環境を提供するために、毎回 `adb` や `fastboot` という接頭辞を入力する必要がありません。ただし、`adb` や `fastboot` という接頭辞が付く標準のコマンドもサポートしています。
*   **直接コマンド**: `ls`、 `rm`、 `install`、 `shell` などのコマンドを直接入力します。
*   **ショートカット**: よく使用する操作を素早く実行するための数字ショートカット (1～9) を提供します。コマンドラインで `s` と入力で Enter を押すとすべてのショートカットが表示されます。
*   **Tab の自動補完**: コマンド、パラメータ、およびデバイスパーティションパスの Tab キーによる自動補完に対応しています。コマンドの一部を入力後に Tab キーを押すと自動補完または一致する候補が表示されます。内蔵のあいまい一致機能とスペル修正機能により、軽微な入力ミスでも正確に認識できます。
*   **シェルモード**: ADB モードで `shell` と入力で Enter キーを押すとシェルモードになります。シェルモードは、シェルコマンドを直接入力できます。`exit` と入力で Enter キーを押すとシェルモードを終了します。シェルモード中に `su` を入力して Enter キーを押すと root シェルに昇格できます。または、`sudo <コマンド>` を使用して単一のコマンドに対して、一時的に権限を昇格することもできます。例えば、`sudo /data/adb/apd module install 1.zip` で一時的な権限昇格を使用してモジュールのインストールで APatch デーモンを呼び出すことができます。 

### 3. 高度な機能 (root マネージャーでシェルアプリに root 権限を付与してください)

#### 🎨 テーマの切り替え
パーソナライズに対応な厳選された 40 種のターミナルテーマスタイルを内蔵しています。
*   **使用方法**: `theme` と入力で Enter キーを押すとテーマ選択画面が表示されます。または `theme <番号>` と入力で特定のテーマに直接的に切り替えができます。
*   **機能**: 各テーマには独自の配色があります。設定は自動的に保存され、次回の起動時に適用されます。テーマはコマンドラインプロンプトの色とスタイルに影響します。

#### ⚡ Sudo による権限昇格
対話型シェルでは `sudo` コマンドが組み込まれています。
*   **使用方法**: `sudo <コマンド>`
*   **ファンクション**: `su -c` を介してコマンドを自動実行し、root 権限を必要とするコマンドを非 root シェル環境で迅速に実行可能にします。
#### 📥 DLI ダウンロードとインストール
組み込みの `dli` (ダウンロードとインストール) コマンドは URL から直接的なモジュールのインストールをサポートしています。
*   **使用方法**: `dli <URL>`
*   **ファンクション**: ファイルを自動的にダウンロードし、デバイスに Push して検出された root ソリューションに従ってインストールします。
#### 🧩 モジュールのインストール (Magisk/KernelSU/APatch)
Supports automated installation of Root modules.
*   **ドラッグ & ドロップでインストール**: Drag a module zip file (`.zip`) directly onto the `FolkAdb.exe` icon. The program will automatically identify the module type and invoke the corresponding installation logic.
*   **スマート検出**: Automatically identifies Magisk, KernelSU, or APatch/FolkPatch environments and calls the appropriate installation logic.

#### 📱 APK をドラッグ&ドロップでインストール
*   **操作**: Drag an `.apk` file directly onto the `FolkAdb.exe` icon or the running window (The window method supports single APK installation, while the icon method supports multiple APKs).
*   **プロセス**: The program automatically identifies the APK file and asks whether to install it. Upon confirmation, the installation process completes automatically.

#### 📂 ファイル転送
*   **ドラッグ & ドロップで転送**: Drag ordinary files onto the `FolkAdb.exe` icon to automatically push them to the device's `/storage/emulated/0/` directory. Supports multiple file selection. If a ZIP file is detected as a module, you will be asked whether to install it.

#### 🔧 Shizuku の有効化
Supports one-click activation of the Shizuku app.
*   **使用方法**: Type `shizuku` and press Enter in the interactive command line.
*   **ファンクション**: Automatically detects the installation status of the Shizuku app on the device and executes the activation command, making it convenient to use the API permissions provided by Shizuku.

## ビルドと実行

### 必須
*   Windows OS
*   C コンパイラー (例: GCC/MinGW)

### ビルド
Run the following command in the project root directory:
```cmd
build.bat
```

### 実行
```cmd
run.bat
```
Or run `build/FolkAdb.exe` directly.

## License
This project is licensed under the Apache 2.0 License. See the [LICENSE](LICENSE) file for details.
